#ifndef GNUOS_SECCOMP_H
#define GNUOS_SECCOMP_H

#include <stdint.h>

#define SECCOMP_ACTION_ALLOW 0U
#define SECCOMP_ACTION_LOG 1U
#define SECCOMP_ACTION_ERRNO 2U

#define SECCOMP_AUDIT_CAPACITY 128U

typedef struct {
    uint64_t sequence;
    uint64_t tid;
    uint64_t syscall_number;
    uint8_t action;
    int64_t result;
} seccomp_audit_event_t;

void seccomp_init(void);
void seccomp_set_default_action(uint8_t action);
int seccomp_set_syscall_action(uint16_t syscall_number, uint8_t action);
uint8_t seccomp_get_syscall_action(uint64_t syscall_number);
void seccomp_audit_record(uint64_t tid, uint64_t syscall_number, uint8_t action, int64_t result);
uint64_t seccomp_audit_count(void);
int seccomp_audit_latest(seccomp_audit_event_t *out_event);

#endif
