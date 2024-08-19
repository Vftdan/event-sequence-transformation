#include "../graph.h"
#include "../module_registry.h"

typedef struct {
	GraphNode as_GraphNode;
	bool has_ns, has_maj, has_min, has_payload;
	EventData source;
} AssignGraphNode;

static bool
handle_event(EventPositionBase * self, EventNode * event)
{
	AssignGraphNode *node = DOWNCAST(AssignGraphNode, GraphNode, DOWNCAST(GraphNode, EventPositionBase, self));
	size_t count = node->as_GraphNode.outputs.length;
	if (!count) {
		event_destroy(event);
		return true;
	}

	if (node->has_ns) {
		event->data.code.ns = node->source.code.ns;
	}
	if (node->has_maj) {
		event->data.code.major = node->source.code.major;
	}
	if (node->has_min) {
		event->data.code.minor = node->source.code.minor;
	}
	if (node->has_payload) {
		event->data.payload = node->source.payload;
	}

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
	AssignGraphNode * node = T_ALLOC(1, AssignGraphNode);
	if (!node) {
		return NULL;
	}

	EventData source;
	bool
		has_ns = false,
		has_maj = false,
		has_min = false,
		has_payload = false;
	const config_setting_t *setting;

	if (config->options) {
		if ((setting = config_setting_get_member(config->options, "namespace"))) {
			has_ns = true;
			source.code.ns = env_resolve_constant(env, setting);
		}
		if ((setting = config_setting_get_member(config->options, "major"))) {
			has_maj = true;
			source.code.major = env_resolve_constant(env, setting);
		}
		if ((setting = config_setting_get_member(config->options, "minor"))) {
			has_min = true;
			source.code.minor = env_resolve_constant(env, setting);
		}
		if ((setting = config_setting_get_member(config->options, "payload"))) {
			has_payload = true;
			source.payload = env_resolve_constant(env, setting);
		}
	} else {
		// Prevent warning for uninitialized variable
		source = (EventData) {
			.code = {
				.ns = 0,
				.major = 0,
				.minor = 0,
			},
			.payload = 0,
		};
	}

	*node = (AssignGraphNode) {
		.as_GraphNode = {
			.as_EventPositionBase = {
				.handle_event = &handle_event,
				.waiting_new_event = false,
			},
			.specification = spec,
			.inputs = EMPTY_GRAPH_CHANNEL_LIST,
			.outputs = EMPTY_GRAPH_CHANNEL_LIST,
		},
		.has_ns = has_ns,
		.has_maj = has_maj,
		.has_min = has_min,
		.has_payload = has_payload,
		.source = source,
	};
	return &node->as_GraphNode;
}

static void destroy
(GraphNodeSpecification * self, GraphNode * target)
{
	(void) self;
	free(target);
}

GraphNodeSpecification nodespec_assign = (GraphNodeSpecification) {
	.create = &create,
	.destroy = &destroy,
	.register_io = NULL,
	.name = "assign",
	.documentation = "Assigns field(s) in an event\nAccepts events on any connector\nSends events on all connectors"
	                 "\nOption 'namespace' (optional): new event code namespace"
	                 "\nOption 'major' (optional): new event code major"
	                 "\nOption 'minor' (optional): new event code minor"
	                 "\nOption 'payload' (optional): new event payload"
	,
};

MODULE_CONSTRUCTOR(init)
{
	register_graph_node_specification(&nodespec_assign);
}
