#include "../graph.h"
#include "../module_registry.h"

typedef struct {
	GraphNode as_GraphNode;
	EventPredicateHandle target;
	EventPredicateHandle enable_on, disable_on, invert_on, uninvert_on;
} ModifyPredicateGraphNode;

static bool
handle_event(EventPositionBase * self, EventNode * event)
{
	ModifyPredicateGraphNode *node = DOWNCAST(ModifyPredicateGraphNode, GraphNode, DOWNCAST(GraphNode, EventPositionBase, self));
	bool
		  should_enable = false,
		  should_disable = false,
		  should_invert = false,
		  should_uninvert = false;
	EventPredicateHandle target = node->target;
	EventPredicate target_state = event_predicate_get(target);

	if (target_state.enabled) {
		should_disable = event_predicate_apply(node->disable_on, event) == EVPREDRES_ACCEPTED;
	} else {
		should_enable = event_predicate_apply(node->enable_on, event) == EVPREDRES_ACCEPTED;
	}
	if (target_state.inverted) {
		should_uninvert = event_predicate_apply(node->uninvert_on, event) == EVPREDRES_ACCEPTED;
	} else {
		should_invert = event_predicate_apply(node->invert_on, event) == EVPREDRES_ACCEPTED;
	}

	if (should_enable) {
		event_predicate_set_enabled(target, true);
	}
	if (should_disable) {
		event_predicate_set_enabled(target, false);
	}
	if (should_invert) {
		event_predicate_set_inverted(target, true);
	}
	if (should_uninvert) {
		event_predicate_set_inverted(target, false);
	}

	event_destroy(event);
	return true;
}

static GraphNode *
create(GraphNodeSpecification * spec, GraphNodeConfig * config, InitializationEnvironment * env)
{
	if (!config->options) {
		return NULL;
	}

	ModifyPredicateGraphNode * node = T_ALLOC(1, ModifyPredicateGraphNode);
	if (!node) {
		return NULL;
	}

	EventPredicateHandle target = env_resolve_event_predicate(env, config_setting_get_member(config->options, "target"));
	if (target < 0) {
		free(node);
		return NULL;
	}

	*node = (ModifyPredicateGraphNode) {
		.as_GraphNode = {
			.as_EventPositionBase = {
				.handle_event = &handle_event,
				.waiting_new_event = false,
			},
			.specification = spec,
			.inputs = EMPTY_GRAPH_CHANNEL_LIST,
			.outputs = EMPTY_GRAPH_CHANNEL_LIST,
		},
		.target = target,
		.enable_on = env_resolve_event_predicate(env, config_setting_get_member(config->options, "enable_on")),
		.disable_on = env_resolve_event_predicate(env, config_setting_get_member(config->options, "disable_on")),
		.invert_on = env_resolve_event_predicate(env, config_setting_get_member(config->options, "invert_on")),
		.uninvert_on = env_resolve_event_predicate(env, config_setting_get_member(config->options, "uninvert_on")),
	};
	return &node->as_GraphNode;
}

static void destroy
(GraphNodeSpecification * self, GraphNode * target)
{
	(void) self;
	free(target);
}

GraphNodeSpecification nodespec_modify_predicate = (GraphNodeSpecification) {
	.create = &create,
	.destroy = &destroy,
	.register_io = NULL,
	.name = "modify_predicate",
	.documentation = "Changes 'enabled' and 'inverted' flags of a predicate\nAccepts events on any connector\nDoes not send events"
	                 "\nOption 'target' (required): the predicate to modify"
	                 "\nOption 'enable_on' (optional): the predicate, satisfying events of which set 'enabled' flag of the target predicate to 1"
	                 "\nOption 'disable_on' (optional): the predicate, satisfying events of which set 'enabled' flag of the target predicate to 0"
	                 "\nOption 'invert_on' (optional): the predicate, satisfying events of which set 'inverted' flag of the target predicate to 1"
	                 "\nOption 'uninvert_on' (optional): the predicate, satisfying events of which set 'inverted' flag of the target predicate to 0"
	,
};

MODULE_CONSTRUCTOR(init)
{
	register_graph_node_specification(&nodespec_modify_predicate);
}
