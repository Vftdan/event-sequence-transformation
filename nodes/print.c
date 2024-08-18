#include <stdio.h>
#include "print.h"
#include "../module_registry.h"

static bool
handle_event(EventPositionBase * self, EventNode * event)
{
#define PRINT_FIELD(fmt, path) printf("%s = " fmt "\n", #path, data.path)
	(void) self;
	EventData data = event->data;
	printf("Event from connector %ld:\n", event->input_index);
	PRINT_FIELD("%d", code.ns);
	PRINT_FIELD("%d", code.major);
	PRINT_FIELD("%d", code.minor);
	PRINT_FIELD("%d", ttl);
	PRINT_FIELD("%d", priority);
	PRINT_FIELD("%ld", payload);
	printf("modifiers = ");
	for (ssize_t i = data.modifiers.byte_length - 1; i >= 0; --i) {
		printf("%02x", data.modifiers.bits[i]);
	}
	printf("\n");
	printf("time.absolute = %ld.%09ld\n", data.time.absolute.tv_sec, data.time.absolute.tv_nsec);
	printf("---\n\n");
	event_destroy(event);
	return true;
#undef PRINT_FIELD
}

static GraphNode *
create(GraphNodeSpecification * spec, GraphNodeConfig * config, InitializationEnvironment * env)
{
	(void) config;
	(void) env;
	GraphNode * node = T_ALLOC(1, GraphNode);
	if (!node) {
		return node;
	}
	*node = (GraphNode) {
		.as_EventPositionBase = {
			.handle_event = &handle_event,
			.waiting_new_event = false,
		},
		.specification = spec,
		.inputs = EMPTY_GRAPH_CHANNEL_LIST,
		.outputs = EMPTY_GRAPH_CHANNEL_LIST,
	};
	return node;
}

static void destroy
(GraphNodeSpecification * self, GraphNode * target)
{
	(void) self;
	free(target);
}

GraphNodeSpecification nodespec_print = (GraphNodeSpecification) {
	.create = &create,
	.destroy = &destroy,
	.register_io = NULL,
	.name = "print",
	.documentation = "Prints received events\nAccepts events on any connector\nDoes not send events"
	,
};

MODULE_CONSTRUCTOR(init)
{
	register_graph_node_specification(&nodespec_print);
}
