#include <assert.h>
#include <string.h>
#include "queue.h"

void
queue_deinit_with_destructor(Queue * q, void (*value_destructor)(QueueValue value, void * destructor_closure), void * destructor_closure)
{
	if (q->values) {
		if (value_destructor) {
			QUEUE_FOREACH_INDEX(i, q) {
				value_destructor(q->values[i], destructor_closure);
			}
		}
		free(q->values);
		q->values = NULL;
	}
	q->capacity = 0;
	q->after_tail_idx = 0;
	q->head_idx = 0;
}

static bool
queue_grow(Queue * queue)
{
	size_t old_capacity = queue->capacity;
	size_t new_capacity = old_capacity + (old_capacity >> 1) + 2;
	QueueValue *values = queue->values;
	if (values) {
		values = T_REALLOC(values, new_capacity, QueueValue);
		if (!values) {
			return false;
		}
		if (queue->head_idx > queue->after_tail_idx) {
			size_t old_head_idx = queue->head_idx;
			size_t head_shift = new_capacity - old_capacity;
			size_t shifted_count = old_capacity - old_head_idx;
			QueueValue *old_head_ptr = &values[old_head_idx];
			memmove(old_head_ptr + head_shift, old_head_ptr, shifted_count * sizeof(QueueValue));
			queue->head_idx = old_head_idx + head_shift;
		}
	} else {
		values = T_ALLOC(new_capacity, QueueValue);
		if (!values) {
			return false;
		}
	}
	queue->values = values;
	queue->capacity = new_capacity;
	return true;
}

ssize_t
queue_put(Queue * queue, QueueValue value)
{
	if (queue_is_full(queue)) {
		if (!queue_grow(queue)) {
			return -1;
		}
		assert(!queue_is_full(queue));
	}
	size_t capacity = queue->capacity;
	assert(capacity > 0);
	size_t index = queue->after_tail_idx;
	queue->values[index] = value;
	queue->after_tail_idx = (index + 1) % capacity;
	return index;
}

bool
queue_try_pop(Queue * queue, QueueValue * ret)
{
	if (!queue_try_peek(queue, ret)) {
		return false;
	}
	queue->head_idx = (queue->head_idx + 1) % queue->capacity;
	return true;
}

bool
queue_try_peek(const Queue * queue, QueueValue * ret)
{
	if (queue_is_empty(queue)) {
		return false;
	}
	assert(queue->capacity > 0);
	if (ret) {
		*ret = queue->values[queue->head_idx];
	}
	return true;
}
