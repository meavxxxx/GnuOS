#ifndef GNUOS_CAPABILITY_H
#define GNUOS_CAPABILITY_H

#include <stdint.h>

#define CAPABILITY_MAX_ENTRIES 128U

#define CAP_RIGHT_READ 0x0001U
#define CAP_RIGHT_WRITE 0x0002U
#define CAP_RIGHT_EXECUTE 0x0004U
#define CAP_RIGHT_TRANSFER 0x0008U

typedef struct {
    uint16_t id;
    uint64_t owner_tid;
    uint64_t object_id;
    uint16_t rights;
} capability_info_t;

void capability_init(void);
int capability_create(
    uint64_t owner_tid,
    uint64_t object_id,
    uint16_t rights,
    uint16_t *out_capability_id);
int capability_validate_transfer(uint16_t capability_id, uint64_t owner_tid, uint16_t rights_mask);
int capability_copy_for_owner(
    uint16_t capability_id,
    uint64_t owner_tid,
    uint16_t rights_mask,
    uint64_t target_tid,
    uint16_t *out_capability_id);
int capability_describe(uint16_t capability_id, capability_info_t *out_info);
uint16_t capability_count(void);

#endif
