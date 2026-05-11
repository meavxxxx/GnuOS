#ifndef GNUOS_SHM_H
#define GNUOS_SHM_H

#include <stdint.h>

#define SHM_MAX_SEGMENTS 32U
#define SHM_SEGMENT_NAME_MAX 32U
#define SHM_MAX_ATTACHMENTS 32U

void shm_init(void);
int shm_create(const char *name, uint64_t size, uint64_t owner_tid);
int shm_find(const char *name);
int shm_attach(int segment_id, uint64_t requester_tid, void **out_address, uint64_t *out_size);
int shm_detach(int segment_id, uint64_t requester_tid);
uint16_t shm_segment_count(void);

#endif
