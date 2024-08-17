#include "../graph.h"
#include "../module_registry.h"

static bool
handle_event(EventPositionBase * self, EventNode * event)
{
	GraphNode *node = DOWNCAST(GraphNode, EventPositionBase, self);
	size_t count = node->outputs.length;
	if (!count) {
		event_destroy(event);
		return true;
	}
	if (count > 1) {
		count = event_replicate(event, count - 1) + 1;
	}
	for (size_t i = 0; i < count; ++i) {
		event->position = &node->outputs.elements[i]->as_EventPositionBase;
		event = event->next;
	}
	return true;
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

GraphNodeSpecification nodespec_tee = (GraphNodeSpecification) {
	.create = &create,
	.destroy = &destroy,
	.register_io = NULL,
	.name = "tee",
};

MODULE_CONSTRUCTOR(init)
{
	register_graph_node_specification(&nodespec_tee);
}
