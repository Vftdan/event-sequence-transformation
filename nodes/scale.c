#include "../graph.h"
#include "../module_registry.h"

typedef struct {
	GraphNode as_GraphNode;
	int64_t numerator;
	int64_t denominator;
	int64_t center;
	int64_t defect;
	bool amortize_rounding_error;
} ScaleGraphNode;

static bool
handle_event(EventPositionBase * self, EventNode * event)
{
	ScaleGraphNode *node = DOWNCAST(ScaleGraphNode, GraphNode, DOWNCAST(GraphNode, EventPositionBase, self));
	size_t count = node->as_GraphNode.outputs.length;
	if (!count) {
		event_destroy(event);
		return true;
	}

	int64_t value = event->data.payload;
	value -= node->center;
	value *= node->numerator;
	if (node->amortize_rounding_error) {
		value += node->defect;
	}
	if (node->denominator) {
		uint64_t undivided = value;
		value /= node->denominator;
		node->defect = undivided - value * node->denominator;
	}
	value += node->center;
	event->data.payload = value;

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
	ScaleGraphNode * node = T_ALLOC(1, ScaleGraphNode);
	if (!node) {
		return NULL;
	}

	int64_t
		numerator = 1,
		denominator = 1,
		center = 0;
	bool amortize_rounding_error = false;
	if (config->options) {
		numerator = env_resolve_constant_or(env, config_setting_get_member(config->options, "numerator"), numerator);
		denominator = env_resolve_constant_or(env, config_setting_get_member(config->options, "denominator"), denominator);
		center = env_resolve_constant_or(env, config_setting_get_member(config->options, "center"), center);
		amortize_rounding_error = env_resolve_constant(env, config_setting_get_member(config->options, "amortize_rounding_error")) != 0;
	}

	*node = (ScaleGraphNode) {
		.as_GraphNode = {
			.as_EventPositionBase = {
				.handle_event = &handle_event,
				.waiting_new_event = false,
			},
			.specification = spec,
			.inputs = EMPTY_GRAPH_CHANNEL_LIST,
			.outputs = EMPTY_GRAPH_CHANNEL_LIST,
		},
		.numerator = numerator,
		.denominator = denominator,
		.center = center,
		.amortize_rounding_error = amortize_rounding_error,
		.defect = 0,
	};
	return &node->as_GraphNode;
}

static void destroy
(GraphNodeSpecification * self, GraphNode * target)
{
	(void) self;
	free(target);
}

GraphNodeSpecification nodespec_scale = (GraphNodeSpecification) {
	.create = &create,
	.destroy = &destroy,
	.register_io = NULL,
	.name = "scale",
	.documentation = "Multiplies event payload by a constant fraction\nAccepts events on any connector\nSends events on all connectors"
	                 "\nOption 'numerator' (optional): an integer to multiply by"
	                 "\nOption 'denominator' (optional): an integer to divide by"
	                 "\nOption 'center' (optional): an integer to scale around"
	                 "\nOption 'amortize_rounding_error' (optional): whether to adjust the new event value by the rounding error of the previous event value"
	,
};

MODULE_CONSTRUCTOR(init)
{
	register_graph_node_specification(&nodespec_scale);
}
