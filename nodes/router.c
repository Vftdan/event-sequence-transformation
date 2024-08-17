#include "../graph.h"
#include "../module_registry.h"

typedef struct {
	GraphNode as_GraphNode;
	size_t length;
	EventPredicateHandle * predicates;
} RouterGraphNode;

static bool
handle_event(EventPositionBase * self, EventNode * event)
{
	RouterGraphNode *node = DOWNCAST(RouterGraphNode, GraphNode, DOWNCAST(GraphNode, EventPositionBase, self));
	for (size_t i = 0; i < node->length; ++i) {
		if (i >= node->as_GraphNode.outputs.length) {
			break;
		}
		if (event_predicate_apply(node->predicates[i], event) == EVPREDRES_ACCEPTED) {
			if (event_replicate(event, 1)) {
				event->next->position = &node->as_GraphNode.outputs.elements[i]->as_EventPositionBase;
			}
		}
	}
	event_destroy(event);
	return true;
}

static GraphNode *
create(GraphNodeSpecification * spec, GraphNodeConfig * config, InitializationEnvironment * env)
{
	(void) config;
	(void) env;
	RouterGraphNode * node = T_ALLOC(1, RouterGraphNode);
	if (!node) {
		return NULL;
	}

	config_setting_t *predicates_setting = config_setting_get_member(config->options, "predicates");
	if (!predicates_setting) {
		free(node);
		return NULL;
	}

	EventPredicateHandle *predicates = NULL;
	size_t length = config_setting_length(predicates_setting);
	if (length > 0) {
		predicates = T_ALLOC(length, EventPredicateHandle);
		if (!predicates) {
			free(node);
			return NULL;
		}
		for (size_t i = 0; i < length; ++i) {
			predicates[i] = env_resolve_event_predicate(env, config_setting_get_elem(predicates_setting, i));
		}
	}

	*node = (RouterGraphNode) {
		.as_GraphNode = {
			.as_EventPositionBase = {
				.handle_event = &handle_event,
				.waiting_new_event = false,
			},
			.specification = spec,
			.inputs = EMPTY_GRAPH_CHANNEL_LIST,
			.outputs = EMPTY_GRAPH_CHANNEL_LIST,
		},
		.length = length,
		.predicates = predicates,
	};
	return &node->as_GraphNode;
}

static void destroy
(GraphNodeSpecification * self, GraphNode * target)
{
	(void) self;
	RouterGraphNode * node = DOWNCAST(RouterGraphNode, GraphNode, target);
	if (node->predicates) {
		free(node->predicates);
		node->predicates = NULL;
		node->length = 0;
	}
	free(target);
}

GraphNodeSpecification nodespec_router = (GraphNodeSpecification) {
	.create = &create,
	.destroy = &destroy,
	.register_io = NULL,
	.name = "router",
};

MODULE_CONSTRUCTOR(init)
{
	register_graph_node_specification(&nodespec_router);
}
