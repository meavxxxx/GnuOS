#ifndef GNUOS_WORKQUEUE_H
#define GNUOS_WORKQUEUE_H

#include <stdint.h>

#include <gnuos/sched.h>
#include <gnuos/spinlock.h>

#define WORKQUEUE_CAPACITY 128U

typedef void (*workqueue_func_t)(void *arg);

typedef struct work_item {
    workqueue_func_t func;
    void *arg;
} work_item_t;

typedef struct work_queue {
    const char *name;
    work_item_t items[WORKQUEUE_CAPACITY];
    uint16_t head;
    uint16_t tail;
    uint16_t size;
    uint64_t submitted;
    uint64_t completed;
    wait_queue_t waiters;
    spinlock_t lock;
} work_queue_t;

void workqueue_init(work_queue_t *queue, const char *name);
int workqueue_submit(work_queue_t *queue, workqueue_func_t func, void *arg);
int workqueue_execute_one(work_queue_t *queue, int block_if_empty);
uint64_t workqueue_pending(work_queue_t *queue);
uint64_t workqueue_submitted(work_queue_t *queue);
uint64_t workqueue_completed(work_queue_t *queue);

#endif
