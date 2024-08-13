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
		replica->data = source->data;
		replica->data.modifiers = modifier_set_copy(source->data.modifiers);
		replica->prev = source;
		replica->next = source->next;
		source->next->next->prev = replica;
		source->next = replica;
	}
	return i;
}

EventNode *
event_create(const EventData * content)
{
	EventNode * event = calloc(1, sizeof(EventNode));
	if (content) {
		event->data = *content;
		event->data.modifiers = modifier_set_copy(content->modifiers);
	} else {
		clock_gettime(CLOCK_MONOTONIC, &event->data.time);
	}
	struct timespec self_time = event->data.time;
	EventNode ** list_pos = &LAST_EVENT;
	FOREACH_EVENT_DESC(other) {
		struct timespec other_time = other->data.time;
		if (self_time.tv_sec > other_time.tv_sec) {
			break;
		}
		if (self_time.tv_sec == other_time.tv_sec) {
			if (self_time.tv_nsec >= other_time.tv_nsec) {
				break;
			}
		}
		list_pos = &other->next->prev;
	}
	EventNode * prev = *list_pos;
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
