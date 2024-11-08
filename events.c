#include <stdio.h>
#include <stdlib.h>
#include "events.h"

EventNode
END_EVENTS = {
	.prev = &END_EVENTS,
	.next = &END_EVENTS,
	.position = NULL,
	.input_index = 0,
};

size_t
event_replicate(EventNode * source, size_t count)
{
	size_t i;
	for (i = 0; i < count; ++i) {
		EventNode * replica = malloc(sizeof(EventNode));
		if (!replica) {
			break;
		}
		replica->position = NULL;
		replica->input_index = 0;
		replica->data = event_data_copy(source->data);
		replica->prev = source;
		replica->next = source->next;
		source->next->prev = replica;
		source->next = replica;
	}
	return i;
}

EventNode *
event_create(const EventData * content)
{
	EventNode * event = T_ALLOC(1, EventNode);
	if (!event) {
		return NULL;
	}
	if (content) {
		event->data = event_data_copy(*content);
	} else {
		event->data.time = get_current_time();
	}
	AbsoluteTime self_time = event->data.time;
	EventNode ** list_pos = &LAST_EVENT;
	FOREACH_EVENT_DESC(other) {
		AbsoluteTime other_time = other->data.time;
		if (absolute_time_cmp(self_time, other_time) >= 0) {
			break;
		}
		list_pos = &other->next->prev;
	}
	EventNode * prev = *list_pos;
	if (!prev) {
		fprintf(stderr, "*list_pos is NULL\n");
		abort();
	}
	event->next = prev->next;
	event->prev = prev;
	prev->next->prev = event;
	prev->next = event;
	return event;
}

void
event_destroy(EventNode * self)
{
	modifier_set_destruct(&self->data.modifiers);
	self->next->prev = self->prev;
	self->prev->next = self->next;
	self->prev = NULL;
	self->next = NULL;
	free(self);
}

void event_destroy_all()
{
	EventNode *ev;
	while ((ev = FIRST_EVENT) != &END_EVENTS) {
		event_destroy(ev);
		if (ev == FIRST_EVENT) {
			fprintf(stderr, "Broken doubly linked event list invariant\n");
			abort();
		}
	}
}
