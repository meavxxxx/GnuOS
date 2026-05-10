#include <stddef.h>
#include <stdint.h>

#include <gnuos/interrupts.h>
#include <gnuos/mm.h>
#include <gnuos/panic.h>
#include <gnuos/sched.h>
#include <gnuos/serial.h>
#include <gnuos/spinlock.h>

#define SCHED_MAX_TASKS 128U
#define SCHED_READY_QUEUE_CAPACITY SCHED_MAX_TASKS
#define SCHED_TASK_INDEX_NONE UINT16_MAX

extern void x86_64_context_switch(task_context_t *from, const task_context_t *to);

static process_t g_processes[SCHED_MAX_TASKS];
static task_t g_tasks[SCHED_MAX_TASKS];
static task_t g_bootstrap_task;
static uint16_t g_ready_queue[SCHED_READY_QUEUE_CAPACITY];
static uint16_t g_ready_head;
static uint16_t g_ready_tail;
static uint16_t g_ready_size;
static uint64_t g_next_pid = 1;
static uint64_t g_next_tid = 1;
static task_t *g_current_task;
static uint16_t g_current_index = SCHED_TASK_INDEX_NONE;
static spinlock_t g_sched_lock;
static uint64_t g_sched_ticks;
static volatile uint8_t g_need_resched;

static uint64_t sched_irq_save(void)
{
    uint64_t rflags = 0;
    __asm__ volatile("pushfq; popq %0; cli" : "=r"(rflags) : : "memory");
    return rflags;
}

static void sched_irq_restore(uint64_t rflags)
{
    if ((rflags & (1ULL << 9U)) != 0ULL) {
        __asm__ volatile("sti" : : : "memory");
    }
}

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

static int sched_pick_next_ready(uint16_t *out_index)
{
    if (!out_index) {
        return 0;
    }

    uint16_t attempts = g_ready_size;
    while (attempts-- > 0U) {
        uint16_t candidate = 0;
        if (!sched_dequeue_task(&candidate)) {
            return 0;
        }

        if (candidate >= SCHED_MAX_TASKS) {
            continue;
        }

        if (g_tasks[candidate].state == TASK_READY) {
            *out_index = candidate;
            return 1;
        }
    }

    return 0;
}

static void sched_reset_task(task_t *task)
{
    if (!task) {
        return;
    }

    task->tid = 0;
    task->state = TASK_UNUSED;
    task->name = NULL;
    task->process = NULL;
    task->entry = NULL;
    task->arg = NULL;
    task->runtime_ticks = 0;
    task->context_switches = 0;
    task->context.rsp = 0;
    task->context.rbx = 0;
    task->context.rbp = 0;
    task->context.r12 = 0;
    task->context.r13 = 0;
    task->context.r14 = 0;
    task->context.r15 = 0;
    task->kernel_stack_base = NULL;
    task->kernel_stack_size = 0;
}

static __attribute__((noreturn)) void sched_task_trampoline(void)
{
    task_t *task = sched_current_task();
    if (!task || !task->entry) {
        kpanic("scheduler trampoline without valid task");
    }

    x86_64_interrupts_enable();
    task->entry(task->arg);
    sched_task_exit();
}

static int sched_prepare_task_stack(task_t *task)
{
    if (!task) {
        return 0;
    }

    task->kernel_stack_size = MM_PAGE_SIZE;
    task->kernel_stack_base = pmm_alloc_page();
    if (!task->kernel_stack_base) {
        task->kernel_stack_size = 0;
        return 0;
    }

    uint64_t stack_top = (uint64_t)(uintptr_t)task->kernel_stack_base + task->kernel_stack_size;
    uint64_t *stack = (uint64_t *)(uintptr_t)stack_top;

    *--stack = (uint64_t)(uintptr_t)sched_task_exit;
    *--stack = (uint64_t)(uintptr_t)sched_task_trampoline;

    task->context.rsp = (uint64_t)(uintptr_t)stack;
    task->context.rbx = 0;
    task->context.rbp = 0;
    task->context.r12 = 0;
    task->context.r13 = 0;
    task->context.r14 = 0;
    task->context.r15 = 0;
    return 1;
}

void sched_init(void)
{
    spinlock_init(&g_sched_lock);

    uint64_t irq_flags = sched_irq_save();
    spinlock_lock(&g_sched_lock);

    for (uint16_t i = 0; i < SCHED_MAX_TASKS; i++) {
        g_processes[i].pid = 0;
        sched_reset_task(&g_tasks[i]);
    }

    g_ready_head = 0;
    g_ready_tail = 0;
    g_ready_size = 0;
    g_next_pid = 1;
    g_next_tid = 1;
    g_sched_ticks = 0;
    g_need_resched = 0;

    sched_reset_task(&g_bootstrap_task);
    g_bootstrap_task.tid = 0;
    g_bootstrap_task.state = TASK_RUNNING;
    g_bootstrap_task.name = "bootstrap";
    g_current_task = &g_bootstrap_task;
    g_current_index = SCHED_TASK_INDEX_NONE;

    spinlock_unlock(&g_sched_lock);
    sched_irq_restore(irq_flags);

    serial_write("GNU OS: scheduler initialized.\n");
}

task_t *sched_current_task(void)
{
    task_t *current = NULL;

    uint64_t irq_flags = sched_irq_save();
    spinlock_lock(&g_sched_lock);
    current = g_current_task;
    spinlock_unlock(&g_sched_lock);
    sched_irq_restore(irq_flags);

    return current;
}

task_t *sched_create_kernel_task(const char *name, kernel_task_entry_t entry, void *arg)
{
    if (!entry) {
        return NULL;
    }

    uint64_t irq_flags = sched_irq_save();
    spinlock_lock(&g_sched_lock);

    uint16_t index = 0;
    task_t *task = sched_alloc_task_slot(&index);
    if (!task) {
        spinlock_unlock(&g_sched_lock);
        sched_irq_restore(irq_flags);
        return NULL;
    }

    process_t *process = &g_processes[index];
    process->pid = g_next_pid++;

    sched_reset_task(task);
    task->tid = g_next_tid++;
    task->state = TASK_BLOCKED;
    task->name = name;
    task->process = process;
    task->entry = entry;
    task->arg = arg;

    spinlock_unlock(&g_sched_lock);
    sched_irq_restore(irq_flags);

    if (!sched_prepare_task_stack(task)) {
        irq_flags = sched_irq_save();
        spinlock_lock(&g_sched_lock);
        process->pid = 0;
        sched_reset_task(task);
        spinlock_unlock(&g_sched_lock);
        sched_irq_restore(irq_flags);
        return NULL;
    }

    irq_flags = sched_irq_save();
    spinlock_lock(&g_sched_lock);

    task->state = TASK_READY;
    if (!sched_enqueue_task(index)) {
        process->pid = 0;
        sched_reset_task(task);
        spinlock_unlock(&g_sched_lock);
        sched_irq_restore(irq_flags);
        return NULL;
    }

    spinlock_unlock(&g_sched_lock);
    sched_irq_restore(irq_flags);
    return task;
}

static void sched_idle_entry(void *arg)
{
    (void)arg;

    for (;;) {
        sched_yield();
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
    uint64_t irq_flags = sched_irq_save();
    spinlock_lock(&g_sched_lock);

    g_sched_ticks++;
    if (g_current_task &&
        g_current_task != &g_bootstrap_task &&
        g_current_task->state == TASK_RUNNING) {
        g_current_task->runtime_ticks++;
        g_need_resched = 1;
    }

    spinlock_unlock(&g_sched_lock);
    sched_irq_restore(irq_flags);
}

void sched_request_resched(void)
{
    g_need_resched = 1;
}

int sched_run(void)
{
    task_t *next = NULL;
    uint16_t next_index = SCHED_TASK_INDEX_NONE;

    uint64_t irq_flags = sched_irq_save();
    spinlock_lock(&g_sched_lock);

    if (g_current_task != &g_bootstrap_task) {
        spinlock_unlock(&g_sched_lock);
        sched_irq_restore(irq_flags);
        return 0;
    }

    if (!sched_pick_next_ready(&next_index)) {
        spinlock_unlock(&g_sched_lock);
        sched_irq_restore(irq_flags);
        return 0;
    }

    next = &g_tasks[next_index];
    next->state = TASK_RUNNING;
    next->context_switches++;
    g_need_resched = 0;
    g_current_task = next;
    g_current_index = next_index;

    spinlock_unlock(&g_sched_lock);
    sched_irq_restore(irq_flags);

    x86_64_context_switch(&g_bootstrap_task.context, &next->context);
    return 1;
}

void sched_yield(void)
{
    task_t *current = NULL;
    uint16_t current_index = SCHED_TASK_INDEX_NONE;

    uint64_t irq_flags = sched_irq_save();
    spinlock_lock(&g_sched_lock);

    current = g_current_task;
    current_index = g_current_index;
    if (!current ||
        current == &g_bootstrap_task ||
        current->state != TASK_RUNNING ||
        current_index == SCHED_TASK_INDEX_NONE) {
        spinlock_unlock(&g_sched_lock);
        sched_irq_restore(irq_flags);
        return;
    }

    current->state = TASK_READY;
    if (!sched_enqueue_task(current_index)) {
        current->state = TASK_RUNNING;
        spinlock_unlock(&g_sched_lock);
        sched_irq_restore(irq_flags);
        return;
    }

    g_current_task = &g_bootstrap_task;
    g_current_index = SCHED_TASK_INDEX_NONE;

    spinlock_unlock(&g_sched_lock);
    sched_irq_restore(irq_flags);

    x86_64_context_switch(&current->context, &g_bootstrap_task.context);
}

__attribute__((noreturn)) void sched_task_exit(void)
{
    task_t *current = NULL;

    uint64_t irq_flags = sched_irq_save();
    spinlock_lock(&g_sched_lock);

    current = g_current_task;
    if (!current || current == &g_bootstrap_task) {
        spinlock_unlock(&g_sched_lock);
        sched_irq_restore(irq_flags);
        kpanic("scheduler exit called without running task");
    }

    current->state = TASK_EXITED;
    current->entry = NULL;
    current->arg = NULL;
    g_current_task = &g_bootstrap_task;
    g_current_index = SCHED_TASK_INDEX_NONE;

    spinlock_unlock(&g_sched_lock);
    sched_irq_restore(irq_flags);

    x86_64_context_switch(&current->context, &g_bootstrap_task.context);
    kpanic("scheduler exit returned unexpectedly");
}

uint64_t sched_ready_count(void)
{
    uint64_t ready = 0;

    uint64_t irq_flags = sched_irq_save();
    spinlock_lock(&g_sched_lock);
    ready = g_ready_size;
    spinlock_unlock(&g_sched_lock);
    sched_irq_restore(irq_flags);

    return ready;
}

uint64_t sched_total_ticks(void)
{
    uint64_t ticks = 0;

    uint64_t irq_flags = sched_irq_save();
    spinlock_lock(&g_sched_lock);
    ticks = g_sched_ticks;
    spinlock_unlock(&g_sched_lock);
    sched_irq_restore(irq_flags);

    return ticks;
}
