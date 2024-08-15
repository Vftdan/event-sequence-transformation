#include <libevdev/libevdev.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <stdio.h>
#include "evdev.h"
#include "../processing.h"
#include "../module_registry.h"

typedef struct {
	GraphNode as_GraphNode;
	IOHandling subscription;
	struct libevdev *dev;
	int fd;
	int namespace;
} EvdevGraphNode;

static void
handle_io(EventPositionBase * self, int fd, bool is_output)
{
	(void) is_output;
	(void) fd;
	EvdevGraphNode * node = DOWNCAST(EvdevGraphNode, GraphNode, DOWNCAST(GraphNode, EventPositionBase, self));
	int err = 0;
	AbsoluteTime realtime;
	clock_gettime(CLOCK_REALTIME, &realtime.absolute);
	AbsoluteTime monotime = get_current_time();
	RelativeTime realtime_adj = absolute_time_sub_absolute(realtime, monotime);
	while (!err) {
		struct input_event buf;
		err = libevdev_next_event(node->dev, LIBEVDEV_READ_FLAG_NORMAL, &buf);
		if (err) {
			if (err == -EAGAIN || err == 1) {
				break;
			}
			node->subscription.enabled = false;
			if (err < 0) {
				errno = -err;
				perror("Failed to read evdev event");
			}
			break;
		}
		realtime.absolute.tv_sec = buf.time.tv_sec;
		realtime.absolute.tv_nsec = buf.time.tv_usec * (long) 1000;
		monotime = absolute_time_sub_relative(realtime, realtime_adj);
		EventData data = {
			.code = {
				.ns = 1,
				.major = buf.type,
				.minor = buf.code,
			},
			.ttl = 100,
			.priority = 10,
			.payload = buf.value,
			.modifiers = EMPTY_MODIFIER_SET,
			.time = monotime,
		};
		for (size_t i = 0; i < node->as_GraphNode.outputs.length; ++i) {
			EventNode *ev = event_create(&data);
			if (!ev) {
				perror("Failed to create event");
				break;
			}
			ev->position = &node->as_GraphNode.outputs.elements[i]->as_EventPositionBase;
		}
	}
}

static GraphNode *
create(GraphNodeSpecification * spec, GraphNodeConfig * config)
{
	EvdevGraphNode * node = T_ALLOC(1, EvdevGraphNode);
	if (!node) {
		return NULL;
	}
	const char *filename = NULL;
	if (config) {
		config_setting_lookup_int(config->options, "namespace", &node->namespace);
		config_setting_lookup_string(config->options, "file", &filename);
	}
	if (filename == NULL) {
		free(node);
		return NULL;
	}
	int fd = open(filename, O_RDONLY | O_NONBLOCK);
	int err;
	if ((err = libevdev_new_from_fd(fd, &node->dev)) < 0) {
		errno = -err;
		free(node);
		return NULL;
	}
	*node = (EvdevGraphNode) {
		.as_GraphNode = {
			.as_EventPositionBase = {
				.handle_event = NULL,
				.waiting_new_event = false,
			},
			.specification = spec,
			.inputs = EMPTY_GRAPH_CHANNEL_LIST,
			.outputs = EMPTY_GRAPH_CHANNEL_LIST,
		},
		.subscription = {
			.self = &node->as_GraphNode.as_EventPositionBase,
			.handle_io = handle_io,
			.enabled = true,
		},
		.dev = node->dev,
		.fd = fd,
		.namespace = node->namespace,
	};
	return &node->as_GraphNode;
}

static void destroy
(GraphNodeSpecification * self, GraphNode * target)
{
	(void) self;
	EvdevGraphNode * node = DOWNCAST(EvdevGraphNode, GraphNode, target);
	if (node->dev) {
		libevdev_free(node->dev);
		node->dev = NULL;
		close(node->fd);
	}
	free(target);
}

static void
register_io(GraphNodeSpecification * self, GraphNode * target, ProcessingState * state)
{
	(void) self;
	EvdevGraphNode * node = DOWNCAST(EvdevGraphNode, GraphNode, target);
	io_subscription_list_add(&state->wait_input, node->fd, &node->subscription);
}

GraphNodeSpecification nodespec_evdev = (GraphNodeSpecification) {
	.create = &create,
	.destroy = &destroy,
	.register_io = &register_io,
	.name = "evdev",
};

MODULE_CONSTRUCTOR(init)
{
	register_graph_node_specification(&nodespec_evdev);
}
