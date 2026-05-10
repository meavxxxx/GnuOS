#ifndef GNUOS_RCU_H
#define GNUOS_RCU_H

#include <stddef.h>
#include <stdint.h>

#define rcu_assign_pointer(ptr, value) __atomic_store_n(&(ptr), (value), __ATOMIC_RELEASE)
#define rcu_dereference(ptr) __atomic_load_n(&(ptr), __ATOMIC_ACQUIRE)
#define rcu_container_of(ptr, type, member) \
    ((type *)((uint8_t *)(ptr) - offsetof(type, member)))

struct rcu_head;
typedef struct rcu_head rcu_head_t;
typedef void (*rcu_callback_t)(rcu_head_t *head);

struct rcu_head {
    rcu_head_t *next;
    rcu_callback_t func;
};

void rcu_init(void);
void rcu_read_lock(void);
void rcu_read_unlock(void);
void synchronize_rcu(void);
void call_rcu(rcu_head_t *head, rcu_callback_t func);
int rcu_process_callbacks(int block_if_empty);
uint64_t rcu_reader_count(void);
uint64_t rcu_callbacks_queued(void);
uint64_t rcu_callbacks_completed(void);

#endif
