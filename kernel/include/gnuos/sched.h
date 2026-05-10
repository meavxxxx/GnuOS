#ifndef GNUOS_SCHED_H
#define GNUOS_SCHED_H

#include <stdint.h>

typedef void (*kernel_task_entry_t)(void *arg);

typedef enum {
    TASK_UNUSED = 0,
    TASK_READY,
    TASK_RUNNING,
    TASK_BLOCKED
} task_state_t;

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
} task_t;

void sched_init(void);
task_t *sched_current_task(void);
task_t *sched_create_kernel_task(const char *name, kernel_task_entry_t entry, void *arg);
task_t *sched_create_idle_task(void);
void sched_tick(void);
uint64_t sched_ready_count(void);
uint64_t sched_total_ticks(void);

#endif
