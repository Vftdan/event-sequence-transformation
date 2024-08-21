#include "../graph.h"
#include "../module_registry.h"

typedef struct {
	GraphNode as_GraphNode;
	int64_t total;
} IntegrateGraphNode;

static bool
handle_event(EventPositionBase * self, EventNode * event)
{
	IntegrateGraphNode *node = DOWNCAST(IntegrateGraphNode, GraphNode, DOWNCAST(GraphNode, EventPositionBase, self));
	size_t count = node->as_GraphNode.outputs.length;
	if (!count) {
		event_destroy(event);
		return true;
	}

	int64_t total = node->total;
	total += event->data.payload;
	event->data.payload = total;
	node->total = total;

	graph_node_broadcast_forward_event(&node->as_GraphNode, event);
	return true;
}

static GraphNode *
create(GraphNodeSpecification * spec, GraphNodeConfig * config, InitializationEnvironment * env)
{
	(void) config;
	(void) env;
	IntegrateGraphNode * node = T_ALLOC(1, IntegrateGraphNode);
	if (!node) {
		return NULL;
	}

	int64_t initial = 0;
	if (config->options) {
		initial = env_resolve_constant(env, config_setting_get_member(config->options, "initial"));
	}

	*node = (IntegrateGraphNode) {
		.as_GraphNode = {
			.as_EventPositionBase = {
				.handle_event = &handle_event,
				.waiting_new_event = false,
			},
			.specification = spec,
			.inputs = EMPTY_GRAPH_CHANNEL_LIST,
			.outputs = EMPTY_GRAPH_CHANNEL_LIST,
		},
		.total = initial,
	};
	return &node->as_GraphNode;
}

static void destroy
(GraphNodeSpecification * self, GraphNode * target)
{
	(void) self;
	free(target);
}

GraphNodeSpecification nodespec_integrate = (GraphNodeSpecification) {
	.create = &create,
	.destroy = &destroy,
	.register_io = NULL,
	.name = "integrate",
	.documentation = "Calculates a running sum of previous event payloads and replaces with it the current one\nAccepts events on any connector\nSends events on all connectors"
	                 "\nOption 'initial' (optional): the initial partial sum value"
	,
};

MODULE_CONSTRUCTOR(init)
{
	register_graph_node_specification(&nodespec_integrate);
}
