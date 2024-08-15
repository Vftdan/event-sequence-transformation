#ifndef EVENTS_H_
#define EVENTS_H_

#include "defs.h"
#include "modifiers.h"
#include "time.h"

typedef uint32_t EventNamespace;

typedef struct {
	EventNamespace ns;
	uint16_t major;
	uint16_t minor;
} EventCode;

typedef struct {
	EventCode code;
	uint32_t ttl;
	int32_t priority;
	int64_t payload;
	ModifierSet modifiers;
	AbsoluteTime time;
} EventData;

typedef struct event_position_base EventPositionBase;
typedef struct event_node EventNode;

struct event_position_base {
	bool (*handle_event) (EventPositionBase * self, EventNode * event);  // If returns false, the scheduler should not rewind back to the start. Must return true if any events were deleted
	bool waiting_new_event;  // Skip from handling until it is set to true. Assigning this position to a event should unset this flag
};

struct event_node {
	EventNode *prev, *next;
	EventPositionBase *position;
	size_t input_index;
	EventData data;
};

extern EventNode END_EVENTS;
#define FIRST_EVENT (END_EVENTS.next)
#define  LAST_EVENT (END_EVENTS.prev)
#define FOREACH_EVENT(ev) for (EventNode *ev = FIRST_EVENT; ev && (ev != &END_EVENTS); ev = ev->next)
#define FOREACH_EVENT_DESC(ev) for (EventNode *ev = LAST_EVENT; ev && (ev != &END_EVENTS); ev = ev->prev)

// Creates count replicas after the source event in the list, position is NULL, returns the number of successfully created replicas
size_t event_replicate(EventNode * source, size_t count);
EventNode * event_create(const EventData * content);
void event_destroy(EventNode * self);
void event_destroy_all();

#endif /* end of include guard: EVENTS_H_ */
