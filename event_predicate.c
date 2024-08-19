#include <limits.h>
#include "event_predicate.h"

typedef struct {
	size_t capacity;
	size_t length;
	EventPredicate *values;
} EventPredicateList;

static EventPredicateList predicates = {
	.length = 0,
	.capacity = 0,
	.values = NULL,
};

static bool
event_predicate_list_extend(EventPredicateList * lst)
{
	size_t capacity = lst->capacity;
	capacity = capacity + (capacity >> 1) + 1;

	EventPredicate *new_values;
	if (lst->values) {
		new_values = T_REALLOC(lst->values, capacity, EventPredicate);
	} else {
		new_values = T_ALLOC(capacity, EventPredicate);
	}
	if (!new_values) {
		return false;
	}
	lst->values = new_values;

	lst->capacity = capacity;
	return true;
}

static void
event_predicate_list_clear(EventPredicateList * lst)
{
	if (lst->values) {
		for (size_t i = 0; i < lst->length; ++i) {
			EventPredicateType type = lst->values[i].type;
			if (type == EVPRED_CONJUNCTION || type == EVPRED_DISJUNCTION) {
				if (lst->values[i].aggregate_data.handles) {
					free(lst->values[i].aggregate_data.handles);
				}
			}
		}
		free(lst->values);
		lst->values = NULL;
	}
	lst->capacity = 0;
	lst->length = 0;
}

EventPredicateHandle
event_predicate_register(EventPredicate predicate)
{
	size_t i = predicates.length;
	if (i >= INT32_MAX) {
		return -1;
	}
	while (i >= predicates.capacity) {
		event_predicate_list_extend(&predicates);
	}
	predicates.values[i] = predicate;
	predicates.length = i + 1;
	return (EventPredicateHandle) i;
}

static EventPredicate*
event_predicate_get_ptr(EventPredicateHandle handle)
{
	if (handle < 0 || (size_t) handle >= predicates.length) {
		return NULL;
	}
	return &predicates.values[handle];
}

EventPredicate
event_predicate_get(EventPredicateHandle handle)
{
	EventPredicate *ptr = event_predicate_get_ptr(handle);
	if (ptr) {
		return *ptr;
	} else {
		return (EventPredicate) {
			.type = EVPRED_INVALID,
			.enabled = false,
			.inverted = false,
		};
	}
}

EventPredicateResult
event_predicate_apply(EventPredicateHandle handle, EventNode * event)
{
	EventPredicate *ptr = event_predicate_get_ptr(handle);
	if (!ptr) {
		return EVPREDRES_DISABLED;
	}
	if (!ptr->enabled) {
		return EVPREDRES_DISABLED;
	}
	bool accepted = false;
	switch (ptr->type) {
	case EVPRED_INVALID:
		return EVPREDRES_DISABLED;
	case EVPRED_ACCEPT:
		accepted = true;
		break;
	case EVPRED_CODE_NS...EVPRED_INPUT_INDEX:
		if (!event) {
			return EVPREDRES_DISABLED;
		}
		{
			int64_t actual;
			switch (ptr->type) {
			case EVPRED_CODE_NS:
				actual = event->data.code.ns;
				break;
			case EVPRED_CODE_MAJOR:
				actual = event->data.code.major;
				break;
			case EVPRED_CODE_MINOR:
				actual = event->data.code.minor;
				break;
			case EVPRED_PAYLOAD:
				actual = event->data.payload;
				break;
			case EVPRED_INPUT_INDEX:
				actual = event->input_index;
				break;
			default:
				return EVPREDRES_DISABLED;
			}
			accepted = ptr->range_data.min_value <= actual && actual <= ptr->range_data.max_value;
		}
		break;
	case EVPRED_CONJUNCTION:
	case EVPRED_DISJUNCTION:
		if (!ptr->aggregate_data.handles) {
			return EVPREDRES_DISABLED;
		}
		{
			bool disjunction = ptr->type == EVPRED_DISJUNCTION;
			accepted = !disjunction;
			for (size_t i = 0; i < ptr->aggregate_data.length; ++i) {
				EventPredicateResult child_result = event_predicate_apply(ptr->aggregate_data.handles[i], event);  // TODO protect from stack overflow
				bool should_stop = false;
				switch (child_result) {
				case EVPREDRES_DISABLED:
					continue;
				case EVPREDRES_ACCEPTED:
					should_stop = false;
					break;
				case EVPREDRES_REJECTED:
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
	case EVPRED_MODIFIER:
		if (!event) {
			return EVPREDRES_DISABLED;
		}
		{
			accepted = modifier_set_has(event->data.modifiers, ptr->single_modifier);
		}
		break;
	default:
		return EVPREDRES_DISABLED;
	}
	if (ptr->inverted) {
		accepted = !accepted;
	}
	return accepted ? EVPREDRES_ACCEPTED : EVPREDRES_REJECTED;
}

void
event_predicate_set_enabled(EventPredicateHandle handle, bool enabled)
{
	EventPredicate *ptr = event_predicate_get_ptr(handle);
	if (!ptr) {
		return;
	}
	ptr->enabled = enabled;
}

void event_predicate_set_inverted(EventPredicateHandle handle, bool inverted)
{
	EventPredicate *ptr = event_predicate_get_ptr(handle);
	if (!ptr) {
		return;
	}
	ptr->inverted = inverted;
}

void
event_predicate_reset()
{
	event_predicate_list_clear(&predicates);
}
