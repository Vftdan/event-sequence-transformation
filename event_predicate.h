#ifndef PREDS_H_
#define PREDS_H_

#include "events.h"

typedef struct event_predicate EventPredicate;
typedef int32_t EventPredicateHandle;

typedef enum {
	EVPRED_INVALID,
	EVPRED_ACCEPT,
	// Range
	EVPRED_CODE_NS,
	EVPRED_CODE_MAJOR,
	EVPRED_CODE_MINOR,
	EVPRED_PAYLOAD,
	// Aggregation
	EVPRED_CONJUNCTION,
	EVPRED_DISJUNCTION,
} EventPredicateType;

typedef enum {
	EVPREDRES_DISABLED = -1,
	EVPREDRES_REJECTED = 0,
	EVPREDRES_ACCEPTED = 1,
} EventPredicateResult;

struct event_predicate {
	EventPredicateType type;
	bool enabled;
	bool inverted;
	union {
		struct {
			int64_t min_value;
			int64_t max_value;
		} range_data;
		struct {
			size_t length;
			EventPredicateHandle *handles;
		} aggregate_data;
	};
};

EventPredicateHandle event_predicate_register(EventPredicate predicate);
EventPredicate event_predicate_get(EventPredicateHandle handle);
EventPredicateResult event_predicate_apply(EventPredicateHandle handle, EventNode * event);
void event_predicate_set_enabled(EventPredicateHandle handle, bool enabled);
void event_predicate_set_inverted(EventPredicateHandle handle, bool inverted);
void event_predicate_reset();

#endif /* end of include guard: PREDS_H_ */
