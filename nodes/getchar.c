#include <stdio.h>
#include <unistd.h>
#include "getchar.h"
#include "../processing.h"
#include "../module_registry.h"

typedef struct {
	GraphNode as_GraphNode;
	IOHandling subscription;
	int32_t namespace;
} GetcharGraphNode;

static void
handle_io(EventPositionBase * self, int fd, bool is_output)
{
	(void) is_output;
	GetcharGraphNode * node = DOWNCAST(GetcharGraphNode, GraphNode, DOWNCAST(GraphNode, EventPositionBase, self));
	char buf[1];
	ssize_t status = read(fd, buf, 1);
	if (status < 0) {
		perror("Failed to read character");
		return;
	}
	EventData data = {
		.code = {
			.ns = 0,
			.major = 0,
			.minor = 1,
		},
		.ttl = 100,
		.priority = 10,
		.payload = (unsigned char) buf[0],
		.modifiers = EMPTY_MODIFIER_SET,
		.time = get_current_time(),
	};
	if (status == 0) {
		node->subscription.enabled = false;
		data.code.minor = 2;
		data.payload = 0;
	}
	for (size_t i = 0; i < node->as_GraphNode.outputs.length; ++i) {
		EventNode *ev = event_create(&data);
		if (!ev) {
			perror("Failed to create event");
			break;
		}
		ev->position = &node->as_GraphNode.outputs.elements[i]->as_EventPositionBase;
	}
}

static GraphNode *
create(GraphNodeSpecification * spec, GraphNodeConfig * config, InitializationEnvironment * env)
{
	GetcharGraphNode * node = T_ALLOC(1, GetcharGraphNode);
	if (!node) {
		return NULL;
	}
	*node = (GetcharGraphNode) {
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
		.namespace = env_resolve_constant(env, config_setting_get_member(config->options, "namespace")),
	};
	return &node->as_GraphNode;
}

static void destroy
(GraphNodeSpecification * self, GraphNode * target)
{
	(void) self;
	free(target);
}

static void
register_io(GraphNodeSpecification * self, GraphNode * target, ProcessingState * state)
{
	(void) self;
	GetcharGraphNode * node = DOWNCAST(GetcharGraphNode, GraphNode, target);
	io_subscription_list_add(&state->wait_input, fileno(stdin), &node->subscription);
}

GraphNodeSpecification nodespec_getchar = (GraphNodeSpecification) {
	.create = &create,
	.destroy = &destroy,
	.register_io = &register_io,
	.name = "getchar",
};

MODULE_CONSTRUCTOR(init)
{
	register_graph_node_specification(&nodespec_getchar);
}
