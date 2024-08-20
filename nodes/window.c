#include <assert.h>
#include <limits.h>
#include "../graph.h"
#include "../module_registry.h"
#include "../queue.h"
#include "../hash_table.h"

typedef TYPED_HASH_TABLE(bool) EventSet;

typedef struct {
	GraphNode as_GraphNode;
	EventData terminator_prototype;
	bool has_terminator;
	bool is_jumping;
	bool has_max_time, has_max_length;
	RelativeTime max_time;
	size_t max_length;
	size_t additional_step;
	size_t skip_next;
	Queue buffer;
	EventSet buffered_set;
} WindowGraphNode;

inline static HashTableKey
hash_event_ptr(const EventNode * ptr) {
	return hash_table_key_from_bytes((void*)&ptr, sizeof(ptr));
}

inline static bool
event_set_has(const EventSet * set, const EventNode * element) {
	return hash_table_find(set, hash_event_ptr(element)) >= 0;
}

inline static bool
event_set_add(EventSet * set, const EventNode * element) {
	const bool value = true;
	return hash_table_insert(set, hash_event_ptr(element), &value) >= 0;
}

inline static bool
event_set_del(EventSet * set, const EventNode * element) {
	return hash_table_delete_by_key(set, hash_event_ptr(element));
}

static EventNode *
replicate_and_advance(EventNode ** ptr)
{
	EventNode * old = *ptr;
	if (!old) {
		return NULL;
	}
	if (event_replicate(old, 1)) {
		*ptr = old->next;
		return old;
	}
	return NULL;
}

static void
trigger_new_window(WindowGraphNode * node, EventNode * base)
{
	if (!base) {
		return;
	}

	if (node->has_terminator) {
		EventNode * terminator = replicate_and_advance(&base);
		if (terminator) {
			terminator->data.code = node->terminator_prototype.code;
			terminator->data.modifiers = modifier_set_copy(node->terminator_prototype.modifiers);
			terminator->data.payload = node->terminator_prototype.payload;
			graph_node_broadcast_forward_event(&node->as_GraphNode, terminator);
		}
	}

	size_t step = 1;
	if (node->is_jumping) {
		step = queue_length(&node->buffer);
	}
	step += node->additional_step;
	if (step < 1) {
		step = 1;
	}

	while (step > 0) {
		QueueValue popped;
		if (!queue_try_pop(&node->buffer, &popped)) {
			break;
		}
		--step;
		EventNode * event = popped.as_ptr;
		event_set_del(&node->buffered_set, event);
	}
	node->skip_next += step;

	QUEUE_FOREACH_INDEX(i, &node->buffer) {
		EventNode *orig = node->buffer.values[i].as_ptr;
		if (!orig) {
			continue;
		}
		EventNode *recipient = replicate_and_advance(&base);
		if (!recipient) {
			continue;
		}
		recipient->data = orig->data;
		recipient->data.modifiers = modifier_set_copy(orig->data.modifiers);
		graph_node_broadcast_forward_event(&node->as_GraphNode, recipient);
	}

	event_destroy(base);
}

static bool
handle_event(EventPositionBase * self, EventNode * event)
{
	WindowGraphNode *node = DOWNCAST(WindowGraphNode, GraphNode, DOWNCAST(GraphNode, EventPositionBase, self));

	if (event_set_has(&node->buffered_set, event)) {
		return false;
	}

	EventNode *replacement;
	const AbsoluteTime new_time = event->data.time;
	if (node->has_max_time) {
		const RelativeTime threshold = node->max_time;
		while (queue_length(&node->buffer) > 0) {
			EventNode *first_event = queue_peek(&node->buffer).as_ptr;
			if (!first_event) {
				break;
			}
			RelativeTime delta = absolute_time_sub_absolute(new_time, first_event->data.time);
			if (relative_time_cmp(delta, threshold) <= 0) {
				break;
			}

			// Here replacement should occur simultaneously-before the event
			replacement = replicate_and_advance(&event);
			if (!replacement) {
				return false;
			}
			event->position = self;
			replacement->position = NULL;
			trigger_new_window(node, replacement);
		}
	}

	if (node->skip_next > 0) {
		--(node->skip_next);
		return true;
	}

	// Replacement should occur after the forwarded replica
	replacement = NULL;
	if (event_replicate(event, 1)) {
		replacement = event->next;
	}

	if (event_replicate(event, 1)) {
		graph_node_broadcast_forward_event(&node->as_GraphNode, event->next);
	}
	queue_put(&node->buffer, (QueueValue){.as_ptr = event});
	event_set_add(&node->buffered_set, event);

	if (node->has_max_length) {
		const size_t threshold = node->max_length;
		while (replacement && queue_length(&node->buffer) >= threshold) {
			trigger_new_window(node, replicate_and_advance(&replacement));
		}
	}

	if (replacement) {
		event_destroy(replacement);
	}
	self->waiting_new_event = true;
	return true;
}

static void
load_event_prototype(InitializationEnvironment * env, const config_setting_t * setting, EventData * proto)
{
	if (!setting) {
		return;
	}

	proto->code.ns = env_resolve_constant(env, config_setting_get_member(setting, "namespace")); 
	proto->code.major = env_resolve_constant(env, config_setting_get_member(setting, "major")); 
	proto->code.minor = env_resolve_constant(env, config_setting_get_member(setting, "minor")); 
	proto->payload = env_resolve_constant(env, config_setting_get_member(setting, "payload")); 
	// TODO modifiers
}

static GraphNode *
create(GraphNodeSpecification * spec, GraphNodeConfig * config, InitializationEnvironment * env)
{
	WindowGraphNode * node = T_ALLOC(1, WindowGraphNode);
	if (!node) {
		return NULL;
	}

	bool is_jumping = false;
	ssize_t additional_step = 0;
	long long max_length = 0;
	long long max_milliseconds = 0;
	EventData terminator = {.code = {0, 0, 0}, .payload = 0, .priority = 10, .modifiers = EMPTY_MODIFIER_SET, .time = {}};
	bool
		has_max_length = false,
		has_max_time = false,
		has_terminator = false;

	if (config->options) {
		is_jumping = env_resolve_constant(env, config_setting_get_member(config->options, "is_jumping")) != 0;

		additional_step = (ssize_t) env_resolve_constant(env, config_setting_get_member(config->options, "additional_step"));
		if (additional_step < 0) {
			additional_step = 0;
		}

		const config_setting_t *setting;

		if ((setting = config_setting_get_member(config->options, "max_length"))) {
			has_max_length = true;
			max_length = env_resolve_constant(env, setting);
			if (max_length < 0) {
				max_length = 0;
			}
			if (max_length > INT32_MAX) {
				max_length = 0;
				has_max_length = false;
			}
		}

		if ((setting = config_setting_get_member(config->options, "max_milliseconds"))) {
			has_max_time = true;
			max_milliseconds = env_resolve_constant(env, setting);
			if (max_milliseconds < 0) {
				max_milliseconds = 0;
			}
		}

		if ((setting = config_setting_get_member(config->options, "terminator"))) {
			has_terminator = true;
			load_event_prototype(env, setting, &terminator);
		}
	}

	*node = (WindowGraphNode) {
		.as_GraphNode = {
			.as_EventPositionBase = {
				.handle_event = &handle_event,
				.waiting_new_event = false,
			},
			.specification = spec,
			.inputs = EMPTY_GRAPH_CHANNEL_LIST,
			.outputs = EMPTY_GRAPH_CHANNEL_LIST,
		},
		.terminator_prototype = terminator,
		.has_terminator = has_terminator,
		.is_jumping = is_jumping,
		.has_max_time = has_max_time,
		.has_max_length = has_max_length,
		.max_time = relative_time_from_millisecond(max_milliseconds),
		.max_length = max_length,
		.additional_step = additional_step,
		.skip_next = 0,
		.buffer = EMPTY_QUEUE,
		.buffered_set = {},
	};
	hash_table_init(&node->buffered_set, NULL);
	return &node->as_GraphNode;
}

static void destroy
(GraphNodeSpecification * self, GraphNode * target)
{
	(void) self;
	WindowGraphNode * node = DOWNCAST(WindowGraphNode, GraphNode, target);
	modifier_set_destruct(&node->terminator_prototype.modifiers);
	queue_deinit(&node->buffer);
	hash_table_deinit(&node->buffered_set);
	free(target);
}

GraphNodeSpecification nodespec_window = (GraphNodeSpecification) {
	.create = &create,
	.destroy = &destroy,
	.register_io = NULL,
	.name = "window",
	.documentation = "Passes events through while copying them into an internal buffer, when buffer buffer.length or (buffer.last.time - buffer.first.time) thresholds are met optionally sends terminator event, rewinds events to buffer start, skips ((is_jumping ? buffer.length : 1) + additional_step) events, retransmits remaining buffered events and starts over\nAccepts events on any connector\nSends events on all connectors"
	                 "\nOption 'is_jumping' (optional): whether to send events at most once"
	                 "\nOption 'additional_step' (optional): natural number --- additional step relative to a regular sliding/jumping window"
	                 "\nOption 'max_length' (optional): natural number --- maximum number of events in a window"
	                 "\nOption 'max_milliseconds' (optional): natural number --- maximum number milliseconds between the first and the last event in a window"
	                 "\nOption 'terminator' (optional): event to send after the window fullness condition is met:"
	                 "\n\tField 'namespace' (optional): set generated event code namespace"
	                 "\n\tField 'major' (optional): set generated event code major"
	                 "\n\tField 'minor' (optional): set generated event code minor"
	                 "\n\tField 'payload' (optional): set generated event payload"
	,
};

MODULE_CONSTRUCTOR(init)
{
	register_graph_node_specification(&nodespec_window);
}
