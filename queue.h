#ifndef QUEUE_H_
#define QUEUE_H_

#include "defs.h"

typedef union {
	void *as_ptr;
	intptr_t as_intptr_t;
	intptr_t as_signed;
	uintptr_t as_uintptr_t;
	uintptr_t as_unsigned;
	uint8_t as_bytes[sizeof(void*)];
} QueueValue;

#define ZERO_QUEUE_VALUE ((QueueValue) {.as_ptr = NULL})

typedef struct {
	// Always keep one unused slot to distinguish empty and full queue
	size_t capacity;
	size_t after_tail_idx;
	size_t head_idx;
	QueueValue *values;
} Queue;

#define EMPTY_QUEUE ((Queue) {.capacity = 0, .after_tail_idx = 0, .head_idx = 0, .values = NULL})

void queue_deinit_with_destructor(Queue * queue, void (*value_destructor)(QueueValue value, void * destructor_closure), void * destructor_closure);
ssize_t queue_put(Queue * queue, QueueValue value);
bool queue_try_pop(Queue * queue, QueueValue * ret);
bool queue_try_peek(const Queue * queue, QueueValue * ret);

__attribute__((unused)) inline static size_t
queue_length(const Queue * queue)
{
	ssize_t n = queue->after_tail_idx - queue->head_idx;
	if (n < 0) {
		n += queue->capacity;
	}
	return n;
}

__attribute__((unused)) inline static bool
queue_is_empty(const Queue * queue)
{
	return queue->head_idx == queue->after_tail_idx;
}

__attribute__((unused)) inline static bool
queue_is_full(const Queue * queue)
{
	size_t capacity = queue->capacity;
	if (!capacity) {
		return true;
	}
	return (queue->after_tail_idx + 1) % capacity == queue->head_idx;
}

__attribute__((unused)) inline static void
queue_deinit(Queue * queue)
{
	queue_deinit_with_destructor(queue, NULL, NULL);
}

__attribute__((unused)) inline static QueueValue
queue_pop(Queue * queue)
{
	QueueValue value = ZERO_QUEUE_VALUE;
	queue_try_pop(queue, &value);
	return value;
}

__attribute__((unused)) inline static QueueValue
queue_peek(const Queue * queue)
{
	QueueValue value = ZERO_QUEUE_VALUE;
	queue_try_peek(queue, &value);
	return value;
}

// As long as head and tail indices are correct, division by zero should not happen
#define QUEUE_FOREACH_INDEX(i, q) for (size_t i = (q)->head_idx; i != (q)->after_tail_idx; i = (i + 1) % (q)->capacity)

#endif /* end of include guard: QUEUE_H_ */
