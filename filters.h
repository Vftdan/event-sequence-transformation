#ifndef FILTERS_H_
#define FILTERS_H_

#include "events.h"

typedef struct event_filter EventFilter;
typedef int32_t EventFilterHandle;

typedef enum {
	EVFILTER_INVALID,
	EVFILTER_ACCEPT,
	// Range
	EVFILTER_CODE_NS,
	EVFILTER_CODE_MAJOR,
	EVFILTER_CODE_MINOR,
	EVFILTER_PAYLOAD,
	// Aggregation
	EVFILTER_CONJUNCTION,
	EVFILTER_DISJUNCTION,
} EventFilterType;

typedef enum {
	EVFILTERRES_DISABLED = -1,
	EVFILTERRES_REJECTED = 0,
	EVFILTERRES_ACCEPTED = 1,
} EventFilterResult;

struct event_filter {
	EventFilterType type;
	bool enabled;
	bool inverted;
	union {
		struct {
			int64_t min_value;
			int64_t max_value;
		} range_data;
		struct {
			size_t length;
			EventFilterHandle *handles;
		} aggregate_data;
	};
};

EventFilterHandle event_filter_register(EventFilter filter);
EventFilter event_filter_get(EventFilterHandle handle);
EventFilterResult event_filter_apply(EventFilterHandle handle, EventNode * event);
void event_filter_set_enabled(EventFilterHandle handle, bool enabled);
void event_filter_set_inverted(EventFilterHandle handle, bool inverted);
void event_filter_reset();

#endif /* end of include guard: FILTERS_H_ */
