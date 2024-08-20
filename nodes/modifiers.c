#include "../graph.h"
#include "../module_registry.h"

typedef struct {
	GraphNode as_GraphNode;
	ModifierSet modifiers;
	ModifierOperation operation;
} ModifiersGraphNode;

static bool
handle_event(EventPositionBase * self, EventNode * event)
{
	ModifiersGraphNode *node = DOWNCAST(ModifiersGraphNode, GraphNode, DOWNCAST(GraphNode, EventPositionBase, self));
	size_t count = node->as_GraphNode.outputs.length;
	if (!count) {
		event_destroy(event);
		return true;
	}
	modifier_set_operation_from(&event->data.modifiers, node->modifiers, node->operation);
	graph_node_broadcast_forward_event(&node->as_GraphNode, event);
	return true;
}

static GraphNode *
create(GraphNodeSpecification * spec, GraphNodeConfig * config, InitializationEnvironment * env)
{
	if (!config->options) {
		return NULL;
	}

	ModifiersGraphNode * node = T_ALLOC(1, ModifiersGraphNode);
	if (!node) {
		return NULL;
	}

	const char *operation_name;
	config_setting_lookup_string(config->options, "operation", &operation_name);
	ModifierOperation op = modifier_operation_parse(operation_name);
	if (op < 0) {
		free(node);
		return NULL;
	}

	config_setting_t *modifiers_setting = config_setting_get_member(config->options, "modifiers");
	if (!modifiers_setting) {
		free(node);
		return NULL;
	}

	ModifierSet modifier_set = EMPTY_MODIFIER_SET;
	size_t length = config_setting_length(modifiers_setting);
	for (size_t i = 0; i < length; ++i) {
		long long mod = env_resolve_constant_or(env, config_setting_get_elem(modifiers_setting, i), -1);
		if (mod < 0 || mod > MODIFIER_MAX) {
			continue;
		}
		modifier_set_set(&modifier_set, mod);
	}

	*node = (ModifiersGraphNode) {
		.as_GraphNode = {
			.as_EventPositionBase = {
				.handle_event = &handle_event,
				.waiting_new_event = false,
			},
			.specification = spec,
			.inputs = EMPTY_GRAPH_CHANNEL_LIST,
			.outputs = EMPTY_GRAPH_CHANNEL_LIST,
		},
		.modifiers = modifier_set,
		.operation = op,
	};
	return &node->as_GraphNode;
}

static void destroy
(GraphNodeSpecification * self, GraphNode * target)
{
	(void) self;
	ModifiersGraphNode * node = DOWNCAST(ModifiersGraphNode, GraphNode, target);
	modifier_set_destruct(&node->modifiers);
	free(target);
}

GraphNodeSpecification nodespec_modifiers = (GraphNodeSpecification) {
	.create = &create,
	.destroy = &destroy,
	.register_io = NULL,
	.name = "modifiers",
	.documentation = "Sets/unsets/toggles modifiers in an event\nAccepts events on any connector\nSends events on all connectors"
	                 "\nOption 'operation' (required): the operation to apply to the event modifier set ('set'/'unset'/'toggle')"
	                 "\nOption 'modifiers' (required): collection of integers --- the set of modifiers to operate on"
	,
};

MODULE_CONSTRUCTOR(init)
{
	register_graph_node_specification(&nodespec_modifiers);
}
