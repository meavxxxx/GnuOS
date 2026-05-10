#include <stddef.h>
#include <stdint.h>

#include <gnuos/sched.h>
#include <gnuos/serial.h>

#define SCHED_MAX_TASKS 128U
#define SCHED_READY_QUEUE_CAPACITY SCHED_MAX_TASKS

static process_t g_processes[SCHED_MAX_TASKS];
static task_t g_tasks[SCHED_MAX_TASKS];
static uint16_t g_ready_queue[SCHED_READY_QUEUE_CAPACITY];
static uint16_t g_ready_head;
static uint16_t g_ready_tail;
static uint16_t g_ready_size;
static uint64_t g_next_pid = 1;
static uint64_t g_next_tid = 1;
static task_t *g_current_task;

static int sched_enqueue_task(uint16_t task_index)
{
    if (g_ready_size >= SCHED_READY_QUEUE_CAPACITY) {
        return 0;
    }

    g_ready_queue[g_ready_tail] = task_index;
    g_ready_tail = (uint16_t)((g_ready_tail + 1U) % SCHED_READY_QUEUE_CAPACITY);
    g_ready_size++;
    return 1;
}

static int sched_dequeue_task(uint16_t *out_task_index)
{
    if (!out_task_index || g_ready_size == 0) {
        return 0;
    }

    *out_task_index = g_ready_queue[g_ready_head];
    g_ready_head = (uint16_t)((g_ready_head + 1U) % SCHED_READY_QUEUE_CAPACITY);
    g_ready_size--;
    return 1;
}

static task_t *sched_alloc_task_slot(uint16_t *out_index)
{
    for (uint16_t i = 0; i < SCHED_MAX_TASKS; i++) {
        if (g_tasks[i].state == TASK_UNUSED) {
            if (out_index) {
                *out_index = i;
            }
            return &g_tasks[i];
        }
    }

    return NULL;
}

void sched_init(void)
{
    for (uint16_t i = 0; i < SCHED_MAX_TASKS; i++) {
        g_processes[i].pid = 0;
        g_tasks[i].tid = 0;
        g_tasks[i].state = TASK_UNUSED;
        g_tasks[i].name = NULL;
        g_tasks[i].process = NULL;
        g_tasks[i].entry = NULL;
        g_tasks[i].arg = NULL;
    }

    g_ready_head = 0;
    g_ready_tail = 0;
    g_ready_size = 0;
    g_next_pid = 1;
    g_next_tid = 1;
    g_current_task = NULL;

    serial_write("GNU OS: scheduler initialized.\n");
}

task_t *sched_current_task(void)
{
    return g_current_task;
}

task_t *sched_create_kernel_task(const char *name, kernel_task_entry_t entry, void *arg)
{
    uint16_t index = 0;
    task_t *task = sched_alloc_task_slot(&index);
    if (!task) {
        return NULL;
    }

    process_t *process = &g_processes[index];
    process->pid = g_next_pid++;

    task->tid = g_next_tid++;
    task->state = TASK_READY;
    task->name = name;
    task->process = process;
    task->entry = entry;
    task->arg = arg;

    if (!sched_enqueue_task(index)) {
        task->state = TASK_UNUSED;
        task->process = NULL;
        process->pid = 0;
        return NULL;
    }

    return task;
}

static void sched_idle_entry(void *arg)
{
    (void)arg;

    for (;;) {
        __asm__ volatile("hlt");
    }
}

task_t *sched_create_idle_task(void)
{
    task_t *idle = sched_create_kernel_task("idle", sched_idle_entry, NULL);
    if (!idle) {
        return NULL;
    }

    serial_write("GNU OS: idle task created.\n");
    return idle;
}

void sched_tick(void)
{
    uint16_t next_index = 0;
    if (!sched_dequeue_task(&next_index)) {
        return;
    }

    task_t *next = &g_tasks[next_index];
    if (next->state != TASK_READY) {
        return;
    }

    if (g_current_task && g_current_task->state == TASK_RUNNING) {
        g_current_task->state = TASK_READY;
        for (uint16_t i = 0; i < SCHED_MAX_TASKS; i++) {
            if (&g_tasks[i] == g_current_task) {
                (void)sched_enqueue_task(i);
                break;
            }
        }
    }

    next->state = TASK_RUNNING;
    g_current_task = next;
}

uint64_t sched_ready_count(void)
{
    return g_ready_size;
}

