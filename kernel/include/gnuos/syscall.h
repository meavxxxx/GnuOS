#ifndef GNUOS_SYSCALL_H
#define GNUOS_SYSCALL_H

#include <stdint.h>

#define SYSCALL_MAX_ENTRIES 512U

#define SYS_SCHED_YIELD 24U
#define SYS_GETTID 186U

typedef struct {
    uint64_t number;
    uint64_t arg0;
    uint64_t arg1;
    uint64_t arg2;
    uint64_t arg3;
    uint64_t arg4;
    uint64_t arg5;
    uint64_t user_rip;
    uint64_t user_rflags;
    uint64_t user_rsp;
} syscall_fastpath_frame_t;

typedef int64_t (*syscall_handler_t)(
    uint64_t arg0,
    uint64_t arg1,
    uint64_t arg2,
    uint64_t arg3,
    uint64_t arg4,
    uint64_t arg5);

void syscall_init(void);
int syscall_register(uint16_t number, syscall_handler_t handler);
int64_t syscall_dispatch(
    uint64_t number,
    uint64_t arg0,
    uint64_t arg1,
    uint64_t arg2,
    uint64_t arg3,
    uint64_t arg4,
    uint64_t arg5);
int64_t syscall_dispatch_fastpath(const syscall_fastpath_frame_t *frame);
uint16_t syscall_registered_count(void);

void x86_64_syscall_fastpath_init(void);
uint8_t x86_64_syscall_fastpath_ready(void);

#endif
