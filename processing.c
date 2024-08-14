#include <stdio.h>
#include <sys/select.h>
#include <assert.h>
#include <limits.h>
#include "processing.h"

static bool
io_subscription_list_extend(IOSubscriptionList * lst)
{
	size_t capacity = lst->capacity;
	capacity = capacity + (capacity >> 1) + 1;

	int * new_fds = reallocarray(lst->fds, capacity, sizeof(int));
	if (!new_fds) {
		return false;
	}
	lst->fds = new_fds;

	IOHandling ** new_subscribers = reallocarray(lst->subscribers, capacity, sizeof(IOHandling*));
	if (!new_subscribers) {
		return false;
	}
	lst->subscribers = new_subscribers;

	lst->capacity = capacity;
	return true;
}

void
io_subscription_list_init(IOSubscriptionList * lst, size_t capacity)
{
	IOSubscriptionList result = {
		.length = 0,
		.capacity = 0,
		.fds = NULL,
		.subscribers = NULL,
	};
	result.fds = calloc(capacity, sizeof(int));
	result.subscribers = calloc(capacity, sizeof(IOHandling*));
	if (!result.fds || !result.subscribers)
		capacity = 0;
	result.capacity = capacity;
	*lst = result;
}

void
io_subscription_list_deinit(IOSubscriptionList * lst)
{
	if (lst->fds)
		free(lst->fds);
	if (lst->subscribers)
		free(lst->subscribers);
	*lst = (IOSubscriptionList) {
		.length = 0,
		.capacity = 0,
		.fds = NULL,
		.subscribers = NULL,
	};
}

void
io_subscription_list_add(IOSubscriptionList * lst, int fd, IOHandling *subscriber)
{
	if (lst->length >= lst->capacity) {
		if (!io_subscription_list_extend(lst)) {
			perror("Failed to extend io subscription list");
			exit(1);
		}
	}
	assert(lst->length < lst->capacity);
	size_t i = lst->length;
	lst->fds[i] = fd;
	lst->subscribers[i] = subscriber;
	lst->length = i + 1;
}

static int
populate_fd_set(fd_set * fds, IOSubscriptionList * src, int old_max_fd)
{
	FD_ZERO(fds);
	for (size_t i = 0; i < src->length; ++i) {
		IOHandling *subscriber = src->subscribers[i];
		if (!subscriber) {
			continue;
		}
		if (!subscriber->enabled) {
			continue;
		}
		int fd = src->fds[i];
		if (fd > old_max_fd) {
			old_max_fd = fd;
		}
		FD_SET(fd, fds);
	}
	return old_max_fd;
}

static void
run_io_handlers(fd_set * fds, IOSubscriptionList * subs, bool arg)
{
	for (size_t i = 0; i < subs->length; ++i) {
		int fd = subs->fds[i];
		if (FD_ISSET(fd, fds)) {
			IOHandling *subscriber = subs->subscribers[i];
			if (!subscriber) {
				continue;
			}
			if (!subscriber->enabled) {
				continue;
			}
			void (*callback) (EventPositionBase*, int, bool) = subscriber->handle_io;
			if (callback) {
				callback(subscriber->self, fd, arg);
			}
		}
	}
}

bool
process_io(ProcessingState * state, const RelativeTime * timeout)
{
	int max_fd = 0;
	fd_set readfds, writefds;

	max_fd = populate_fd_set(&readfds, &state->wait_input, max_fd);
	max_fd = populate_fd_set(&writefds, &state->wait_output, max_fd);

	++max_fd;
	int ready = pselect(max_fd, &readfds, &writefds, NULL, &timeout->relative, NULL);

	if (ready < 0) {
		FD_ZERO(&readfds);
		FD_ZERO(&writefds);
		return false;
	}

	run_io_handlers(&readfds, &state->wait_input, false);
	run_io_handlers(&writefds, &state->wait_output, true);

	FD_ZERO(&readfds);
	FD_ZERO(&writefds);
	return true;
}

bool
schedule_delay(ProcessingState * state, EventPositionBase * target, void (*callback) (EventPositionBase*, void*, const AbsoluteTime*), const AbsoluteTime * time)
{
	DelayList **next = &state->wait_delay;
	while (*next) {
		AbsoluteTime next_time = (*next)->time;
		if (absolute_time_cmp(next_time, *time) > 0) {
			break;
		}
		next = &((*next)->next);
	}

	DelayList * current = malloc(sizeof(DelayList));
	if (!current) {
		return false;
	}

	*current = (DelayList) {
		.callback = callback,
		.target = target,
		.next = *next,
		.time = *time,
	};
	*next = current;
	return true;
}

static const RelativeTime ZERO_TO = {
	.relative ={
		.tv_sec = 0,
		.tv_nsec = 0,
	}
};

static bool
process_single_scheduled(ProcessingState * state, const AbsoluteTime extern_time)
{
	if (!state->wait_delay) {
		return false;
	}
	AbsoluteTime next_scheduled_time = state->wait_delay->time;
	if (absolute_time_cmp(next_scheduled_time, extern_time) > 0) {
		return false;
	}
	DelayList next_scheduled = *state->wait_delay;
	free(state->wait_delay);
	state->wait_delay = next_scheduled.next;

	if (next_scheduled.callback) {
		next_scheduled.callback(
			next_scheduled.target,
			next_scheduled.closure,
			&next_scheduled_time
		);
	}
	return true;
}

static bool
process_events_until(ProcessingState * state, const AbsoluteTime * max_time)
{
	bool stable = true;
	int32_t next_priority = INT32_MIN;
	state->has_future_events = false;

	FOREACH_EVENT(ev) {
		if (max_time) {
			AbsoluteTime ev_time = ev->data.time;
			if (absolute_time_cmp(ev_time, *max_time) > 0) {
				// stable = false;
				state->has_future_events = true;
				break;
			}
		}

		if (ev->data.priority > next_priority) {
			next_priority = ev->data.priority;
		}
	}

	while (next_priority > INT32_MIN) {
		state->pass_priority = next_priority;
		next_priority = INT32_MIN;
		FOREACH_EVENT(ev) {
			int32_t ev_priority = ev->data.priority;
			if (ev_priority < state->pass_priority) {
				if (ev_priority > next_priority) {
					next_priority = ev_priority;
				}
			} else if (ev_priority > state->pass_priority) {
				continue;
			}

			EventPositionBase *position = ev->position;
			if (!position) {
				continue;
			}
			if (position->waiting_new_event) {
				continue;
			}
			bool (*handler) (EventPositionBase*, EventNode*) = position->handle_event;
			if (!handler) {
				continue;
			}

			if (max_time) {
				AbsoluteTime ev_time = ev->data.time;
				if (absolute_time_cmp(ev_time, *max_time) > 0) {
					state->has_future_events = true;
					break;
				}
			}

			stable = false;
			bool should_rewind = handler(position, ev);
			if (should_rewind) {
				// ev = &END_EVENTS;  // Will be set to FIRST_EVENT by loop increment
				next_priority = INT32_MIN;  // Break out of the outermost loop
				break;
			}
		}
	}

	state->reached_time = *max_time;
	FOREACH_EVENT(ev) {
		state->reached_time = ev->data.time;
		break;
	}

	return !stable;
}

void
process_iteration(ProcessingState * state)
{
	AbsoluteTime extern_time = get_current_time();

	// late_by.tv_sec = extern_time.tv_sec - state->reached_time.tv_sec;
	// late_by.tv_nsec = extern_time.tv_nsec - state->reached_time.tv_nsec;
	// fix_nsec(&late_by);

	RelativeTime next_scheduled_delay;
	const RelativeTime *max_io_timeout = NULL;
	if (state->has_future_events) {
		max_io_timeout = &ZERO_TO;
	} else {
		if (state->wait_delay) {
			next_scheduled_delay = absolute_time_sub_absolute(state->wait_delay->time, extern_time);
			if (relative_time_cmp(next_scheduled_delay, ZERO_TO) < 0) {
				max_io_timeout = &ZERO_TO;
			} else {
				max_io_timeout = &next_scheduled_delay;
			}
		}
	}

	// FIXME reason about timeouts
	process_io(state, max_io_timeout);
	// process_io(state, &ZERO_TO);

	while (true) {
		bool had_scheduled = process_single_scheduled(state, extern_time);
		const AbsoluteTime *max_event_time = &extern_time;
		if (state->wait_delay) {
			AbsoluteTime next_scheduled_time = state->wait_delay->time;
			bool use_scheduled = false;
			if (!use_scheduled) {
				use_scheduled = absolute_time_cmp(next_scheduled_time, extern_time) > 0;
			}
			if (use_scheduled) {
				max_event_time = &state->wait_delay->time;
			}
		}
		bool had_events = process_events_until(state, max_event_time);
		if (!had_scheduled && !had_events) {
			break;
		}
		process_io(state, &ZERO_TO);
	}
}
