#include <stdio.h>
#include "graph.h"

static void
graph_channel_list_resize(GraphChannelList * lst, size_t target)
{
	if (target > lst->length) {
		GraphChannel **new_elements = T_REALLOC(lst->elements, target, GraphChannel*);
		if (!new_elements) {
			return;
		}
		memset(&new_elements[lst->length], 0, (target - lst->length) * sizeof(GraphChannel*));
		lst->elements = new_elements;
	}
	lst->length = target;
}

inline static void
graph_channel_list_ensure(GraphChannelList * lst, size_t idx)
{
	size_t min_length = idx + 1;
	if (lst->length < min_length) {
		graph_channel_list_resize(lst, min_length);
	}
	if (lst->length < min_length) {
		perror("Failed to resize graph channel list");
		exit(1);
	}
}

static bool
channel_handle_event(EventPositionBase * self, EventNode * event)
{
	GraphChannel *ch = DOWNCAST(GraphChannel, EventPositionBase, self);
	if (event->data.ttl == 0) {
		event_destroy(event);
		return true;
	}
	event->data.ttl--;
	if (event->data.ttl == 0) {
		event_destroy(event);
		return true;
	}
	GraphNode * target = ch->end;
	if (!target) {
		event_destroy(event);
		return true;
	}
	event->position = &target->as_EventPositionBase;
	event->input_index = ch->idx_end;
	target->as_EventPositionBase.waiting_new_event = false;
	return false;  // Continue processing events
}

void
graph_channel_init(GraphChannel * ch, GraphNode * start, size_t start_idx, GraphNode * end, size_t end_idx)
{
	if (start) {
		graph_channel_list_ensure(&start->outputs, start_idx);
		GraphChannel ** old_ch = &start->outputs.elements[start_idx];
		if (*old_ch) {
			if ((*old_ch)->start == start && (*old_ch)->idx_start == start_idx) {
				(*old_ch)->start = NULL;
				// TODO on orphaned check
			}
		}
		*old_ch = ch;
	}

	if (end) {
		graph_channel_list_ensure(&end->inputs, end_idx);
		GraphChannel ** old_ch = &end->inputs.elements[end_idx];
		if (*old_ch) {
			if ((*old_ch)->end == end && (*old_ch)->idx_end == end_idx) {
				(*old_ch)->end = NULL;
				// TODO on orphaned check
			}
		}
		*old_ch = ch;
	}

	ch->start = start;
	ch->end = end;
	ch->idx_start = start_idx;
	ch->idx_end = end_idx;
	ch->as_EventPositionBase.handle_event = &channel_handle_event;
	ch->as_EventPositionBase.waiting_new_event = false;
}

GraphNode *
graph_node_new(GraphNodeSpecification * spec, GraphNodeConfig * config, InitializationEnvironment * env)
{
	if (!spec || !spec->create) {
		return NULL;
	}
	return spec->create(spec, config, env);
}

void
graph_node_delete(GraphNode * self)
{
	if (!self) {
		return;
	}

	for (size_t i = 0; i < self->inputs.length; ++i) {
		GraphChannel *ch = self->inputs.elements[i];
		if (ch && ch->end == self) {
			ch->end = NULL;
			if (!ch->start && !ch->end) {
				// free(ch);  // TODO on orphaned
			}
		}
		self->inputs.elements[i] = NULL;
	}

	for (size_t i = 0; i < self->outputs.length; ++i) {
		GraphChannel *ch = self->outputs.elements[i];
		if (ch && ch->start == self) {
			ch->start = NULL;
			if (!ch->start && !ch->end) {
				// free(ch);  // TODO on orphaned
			}
		}
		self->outputs.elements[i] = NULL;
	}

	graph_channel_list_deinit(&self->inputs);
	graph_channel_list_deinit(&self->outputs);

	GraphNodeSpecification *spec = self->specification;
	if (!spec || !spec->destroy) {
		return;
	}

	spec->destroy(spec, self);
}

void
graph_node_register_io(GraphNode * self, ProcessingState * state)
{
	if (!self) {
		return;
	}
	GraphNodeSpecification *spec = self->specification;
	if (!spec || !spec->register_io) {
		return;
	}
	spec->register_io(spec, self, state);
}

void
graph_channel_list_init(GraphChannelList * lst)
{
	lst->length = 0;
	lst->elements = NULL;
}

void
graph_channel_list_deinit(GraphChannelList * lst)
{
	if (lst->elements) {
		free(lst->elements);
		lst->elements = NULL;
	}
	lst->length = 0;
}
