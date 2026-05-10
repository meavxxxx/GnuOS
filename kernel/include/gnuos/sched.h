#ifndef GNUOS_SCHED_H
#define GNUOS_SCHED_H

#include <stdint.h>

#define SCHED_WAIT_QUEUE_CAPACITY 128U

typedef void (*kernel_task_entry_t)(void *arg);

typedef enum {
    TASK_UNUSED = 0,
    TASK_READY,
    TASK_RUNNING,
    TASK_BLOCKED,
    TASK_EXITED
} task_state_t;

typedef struct task_context {
    uint64_t rsp;
    uint64_t rbx;
    uint64_t rbp;
    uint64_t r12;
    uint64_t r13;
    uint64_t r14;
    uint64_t r15;
} task_context_t;

typedef struct process {
    uint64_t pid;
} process_t;

typedef struct task {
    uint64_t tid;
    task_state_t state;
    const char *name;
    process_t *process;
    kernel_task_entry_t entry;
    void *arg;
    uint64_t runtime_ticks;
    uint64_t context_switches;
    task_context_t context;
    void *kernel_stack_base;
    uint64_t kernel_stack_size;
} task_t;

typedef struct wait_queue {
    uint16_t queue[SCHED_WAIT_QUEUE_CAPACITY];
    uint16_t head;
    uint16_t tail;
    uint16_t size;
} wait_queue_t;

void sched_init(void);
task_t *sched_current_task(void);
task_t *sched_create_kernel_task(const char *name, kernel_task_entry_t entry, void *arg);
task_t *sched_create_idle_task(void);
void sched_wait_queue_init(wait_queue_t *queue);
int sched_wait_queue_wait(wait_queue_t *queue);
int sched_wait_queue_wake_one(wait_queue_t *queue);
uint64_t sched_wait_queue_wake_all(wait_queue_t *queue);
void sched_tick(void);
void sched_request_resched(void);
int sched_run(void);
void sched_yield(void);
__attribute__((noreturn)) void sched_task_exit(void);
uint64_t sched_ready_count(void);
uint64_t sched_total_ticks(void);

#endif
