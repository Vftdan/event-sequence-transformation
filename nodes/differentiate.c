#include "../graph.h"
#include "../module_registry.h"

typedef struct {
	GraphNode as_GraphNode;
	int64_t previous;
} DifferentiateGraphNode;

static bool
handle_event(EventPositionBase * self, EventNode * event)
{
	DifferentiateGraphNode *node = DOWNCAST(DifferentiateGraphNode, GraphNode, DOWNCAST(GraphNode, EventPositionBase, self));
	size_t count = node->as_GraphNode.outputs.length;
	if (!count) {
		event_destroy(event);
		return true;
	}

	int64_t current = event->data.payload;
	event->data.payload = current - node->previous;
	node->previous = current;

	if (count > 1) {
		count = event_replicate(event, count - 1) + 1;
	}
	for (size_t i = 0; i < count; ++i) {
		event->position = &node->as_GraphNode.outputs.elements[i]->as_EventPositionBase;
		if (!event->position) {
			EventNode *orphaned = event;
			event = orphaned->prev;
			event_destroy(orphaned);
		}
		event = event->next;
	}
	return true;
}

static GraphNode *
create(GraphNodeSpecification * spec, GraphNodeConfig * config, InitializationEnvironment * env)
{
	(void) config;
	(void) env;
	DifferentiateGraphNode * node = T_ALLOC(1, DifferentiateGraphNode);
	if (!node) {
		return NULL;
	}

	int64_t initial = 0;
	if (config->options) {
		initial = env_resolve_constant(env, config_setting_get_member(config->options, "initial"));
	}

	*node = (DifferentiateGraphNode) {
		.as_GraphNode = {
			.as_EventPositionBase = {
				.handle_event = &handle_event,
				.waiting_new_event = false,
			},
			.specification = spec,
			.inputs = EMPTY_GRAPH_CHANNEL_LIST,
			.outputs = EMPTY_GRAPH_CHANNEL_LIST,
		},
		.previous = initial,
	};
	return &node->as_GraphNode;
}

static void destroy
(GraphNodeSpecification * self, GraphNode * target)
{
	(void) self;
	free(target);
}

GraphNodeSpecification nodespec_differentiate = (GraphNodeSpecification) {
	.create = &create,
	.destroy = &destroy,
	.register_io = NULL,
	.name = "differentiate",
	.documentation = "Subtracts the previous event payload from the current one\nAccepts events on any connector\nSends events on all connectors"
	                 "\nOption 'initial' (optional): the value to subtract from the first event payload"
	,
};

MODULE_CONSTRUCTOR(init)
{
	register_graph_node_specification(&nodespec_differentiate);
}
