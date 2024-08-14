#ifndef PROCESSING_H_
#define PROCESSING_H_

#include <sys/select.h>
#include "events.h"

typedef struct io_handling IOHandling;

// no virtual multiinheritance
struct io_handling {
	EventPositionBase * self;
	void (*handle_io) (EventPositionBase * self, int fd, bool is_output);
	bool enabled;
};

typedef struct {
	size_t length;
	size_t capacity;
	int *fds;
	IOHandling **subscribers;
} IOSubscriptionList;

typedef struct delay_list DelayList;

struct delay_list {
	void (*callback) (EventPositionBase * target, void * closure, const AbsoluteTime * time);
	EventPositionBase *target;
	void *closure;
	DelayList *next;
	AbsoluteTime time;
};

typedef struct {
	IOSubscriptionList wait_input, wait_output;
	DelayList *wait_delay;
	AbsoluteTime reached_time;
	int32_t pass_priority;
	bool has_future_events;
} ProcessingState;

void io_subscription_list_init(IOSubscriptionList * lst, size_t capacity);
void io_subscription_list_deinit(IOSubscriptionList * lst);
void io_subscription_list_add(IOSubscriptionList * lst, int fd, IOHandling *subscriber);

bool schedule_delay(ProcessingState * state, EventPositionBase * target, void (*callback) (EventPositionBase*, void*, const AbsoluteTime*), const AbsoluteTime * time);
bool process_io(ProcessingState * state, const RelativeTime * timeout);
void process_iteration(ProcessingState * state);

#endif /* end of include guard: PROCESSING_H_ */
