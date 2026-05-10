#include <stdint.h>

#include <gnuos/sched.h>
#include <gnuos/workqueue.h>

static int workqueue_pop_locked(work_queue_t *queue, work_item_t *out_item)
{
    if (!queue || !out_item || queue->size == 0U) {
        return 0;
    }

    *out_item = queue->items[queue->head];
    queue->head = (uint16_t)((queue->head + 1U) % WORKQUEUE_CAPACITY);
    queue->size--;
    return 1;
}

void workqueue_init(work_queue_t *queue, const char *name)
{
    if (!queue) {
        return;
    }

    queue->name = name;
    queue->head = 0;
    queue->tail = 0;
    queue->size = 0;
    queue->submitted = 0;
    queue->completed = 0;
    sched_wait_queue_init(&queue->waiters);
    spinlock_init(&queue->lock);
}

int workqueue_submit(work_queue_t *queue, workqueue_func_t func, void *arg)
{
    if (!queue || !func) {
        return 0;
    }

    int submitted = 0;

    spinlock_lock(&queue->lock);
    if (queue->size < WORKQUEUE_CAPACITY) {
        queue->items[queue->tail].func = func;
        queue->items[queue->tail].arg = arg;
        queue->tail = (uint16_t)((queue->tail + 1U) % WORKQUEUE_CAPACITY);
        queue->size++;
        queue->submitted++;
        submitted = 1;
    }
    spinlock_unlock(&queue->lock);

    if (submitted) {
        (void)sched_wait_queue_wake_one(&queue->waiters);
    }

    return submitted;
}

int workqueue_execute_one(work_queue_t *queue, int block_if_empty)
{
    if (!queue) {
        return 0;
    }

    for (;;) {
        work_item_t item = {0};
        int have_item = 0;

        spinlock_lock(&queue->lock);
        have_item = workqueue_pop_locked(queue, &item);
        spinlock_unlock(&queue->lock);

        if (!have_item) {
            if (!block_if_empty) {
                return 0;
            }

            if (!sched_wait_queue_wait(&queue->waiters)) {
                sched_yield();
            }
            continue;
        }

        item.func(item.arg);

        spinlock_lock(&queue->lock);
        queue->completed++;
        spinlock_unlock(&queue->lock);
        return 1;
    }
}

uint64_t workqueue_pending(work_queue_t *queue)
{
    if (!queue) {
        return 0;
    }

    uint64_t pending = 0;
    spinlock_lock(&queue->lock);
    pending = queue->size;
    spinlock_unlock(&queue->lock);
    return pending;
}

uint64_t workqueue_submitted(work_queue_t *queue)
{
    if (!queue) {
        return 0;
    }

    uint64_t submitted = 0;
    spinlock_lock(&queue->lock);
    submitted = queue->submitted;
    spinlock_unlock(&queue->lock);
    return submitted;
}

uint64_t workqueue_completed(work_queue_t *queue)
{
    if (!queue) {
        return 0;
    }

    uint64_t completed = 0;
    spinlock_lock(&queue->lock);
    completed = queue->completed;
    spinlock_unlock(&queue->lock);
    return completed;
}
