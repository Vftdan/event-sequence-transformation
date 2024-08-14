#include <limits.h>
#include "filters.h"

typedef struct {
	size_t capacity;
	size_t length;
	EventFilter *values;
} EventFilterList;

static EventFilterList filters = {
	.length = 0,
	.capacity = 0,
	.values = NULL,
};

static bool
event_filter_list_extend(EventFilterList * lst)
{
	size_t capacity = lst->capacity;
	capacity = capacity + (capacity >> 1) + 1;

	EventFilter *new_values;
	if (lst->values) {
		new_values = reallocarray(lst->values, capacity, sizeof(EventFilter));
	} else {
		new_values = calloc(capacity, sizeof(EventFilter));
	}
	if (!new_values) {
		return false;
	}
	lst->values = new_values;

	lst->capacity = capacity;
	return true;
}

static void
event_filter_list_clear(EventFilterList * lst)
{
	if (lst->values) {
		free(lst->values);
		lst->values = NULL;
	}
	lst->capacity = 0;
	lst->length = 0;
}

EventFilterHandle
event_filter_register(EventFilter filter)
{
	size_t i = filters.length;
	if (i >= INT32_MAX) {
		return -1;
	}
	while (i <= filters.capacity) {
		event_filter_list_extend(&filters);
	}
	filters.values[i] = filter;
	filters.length = i + 1;
	return (EventFilterHandle) i;
}

static EventFilter*
event_filter_get_ptr(EventFilterHandle handle)
{
	if (handle < 0 || (size_t) handle >= filters.length) {
		return NULL;
	}
	return &filters.values[handle];
}

EventFilter
event_filter_get(EventFilterHandle handle)
{
	EventFilter *ptr = event_filter_get_ptr(handle);
	if (ptr) {
		return *ptr;
	} else {
		return (EventFilter) {
			.type = EVFILTER_INVALID,
			.enabled = false,
			.inverted = false,
		};
	}
}

EventFilterResult
event_filter_apply(EventFilterHandle handle, EventNode * event)
{
	EventFilter *ptr = event_filter_get_ptr(handle);
	if (!ptr) {
		return EVFILTERRES_DISABLED;
	}
	if (!ptr->enabled) {
		return EVFILTERRES_DISABLED;
	}
	bool accepted = false;
	switch (ptr->type) {
	case EVFILTER_INVALID:
		return EVFILTERRES_DISABLED;
	case EVFILTER_ACCEPT:
		accepted = true;
		break;
	case EVFILTER_CODE_NS...EVFILTER_PAYLOAD:
		if (!event) {
			return EVFILTERRES_DISABLED;
		}
		{
			int64_t actual;
			switch (ptr->type) {
			case EVFILTER_CODE_NS:
				actual = event->data.code.ns;
				break;
			case EVFILTER_CODE_MAJOR:
				actual = event->data.code.major;
				break;
			case EVFILTER_CODE_MINOR:
				actual = event->data.code.minor;
				break;
			case EVFILTER_PAYLOAD:
				actual = event->data.payload;
				break;
			default:
				return EVFILTERRES_DISABLED;
			}
			accepted = ptr->range_data.min_value <= actual && actual <= ptr->range_data.max_value;
		}
		break;
	case EVFILTER_CONJUNCTION:
	case EVFILTER_DISJUNCTION:
		if (!ptr->aggregate_data.handles) {
			return EVFILTERRES_DISABLED;
		}
		{
			bool disjunction = ptr->type == EVFILTER_DISJUNCTION;
			accepted = !disjunction;
			for (size_t i = 0; i < ptr->aggregate_data.length; ++i) {
				EventFilterResult child_result = event_filter_apply(ptr->aggregate_data.handles[i], event);  // TODO protect from stack overflow
				bool should_stop = false;
				switch (child_result) {
				case EVFILTERRES_DISABLED:
					continue;
				case EVFILTERRES_ACCEPTED:
					should_stop = false;
					break;
				case EVFILTERRES_REJECTED:
					should_stop = true;
					break;
				}
				if (disjunction) {
					should_stop = !should_stop;
				}
				if (should_stop) {
					accepted = disjunction;
					break;
				}
			}
		}
		break;
	}
	if (ptr->inverted) {
		accepted = !accepted;
	}
	return accepted ? EVFILTERRES_ACCEPTED : EVFILTERRES_REJECTED;
}

void
event_filter_set_enabled(EventFilterHandle handle, bool enabled)
{
	EventFilter *ptr = event_filter_get_ptr(handle);
	if (!ptr) {
		return;
	}
	ptr->enabled = enabled;
}

void event_filter_set_inverted(EventFilterHandle handle, bool inverted)
{
	EventFilter *ptr = event_filter_get_ptr(handle);
	if (!ptr) {
		return;
	}
	ptr->inverted = inverted;
}

void
event_filter_reset()
{
	event_filter_list_clear(&filters);
}
