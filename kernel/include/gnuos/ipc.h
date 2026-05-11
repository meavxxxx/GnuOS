#ifndef GNUOS_IPC_H
#define GNUOS_IPC_H

#include <stdint.h>

#define IPC_MAX_CHANNELS 16U
#define IPC_CHANNEL_NAME_MAX 32U
#define IPC_CHANNEL_QUEUE_CAPACITY 32U
#define IPC_MESSAGE_DATA_MAX 64U

typedef struct {
    uint64_t sender_tid;
    uint16_t size;
    uint8_t has_capability;
    uint16_t capability_id;
    uint16_t capability_rights;
    uint8_t data[IPC_MESSAGE_DATA_MAX];
} ipc_message_t;

void ipc_init(void);
int ipc_channel_create(const char *name);
int ipc_channel_find(const char *name);
int ipc_channel_send(int channel_id, const void *payload, uint16_t size, uint64_t sender_tid);
int ipc_channel_send_capability(
    int channel_id,
    const void *payload,
    uint16_t size,
    uint64_t sender_tid,
    uint16_t capability_id,
    uint16_t capability_rights);
int ipc_channel_recv(
    int channel_id,
    void *payload,
    uint16_t payload_capacity,
    uint16_t *out_size,
    uint64_t *out_sender_tid);
int ipc_channel_recv_capability(
    int channel_id,
    void *payload,
    uint16_t payload_capacity,
    uint16_t *out_size,
    uint64_t *out_sender_tid,
    uint16_t *out_capability_id,
    uint16_t *out_capability_rights);
int ipc_channel_rendezvous_send(int channel_id, const void *payload, uint16_t size, uint64_t sender_tid);
int ipc_channel_rendezvous_recv(
    int channel_id,
    void *payload,
    uint16_t payload_capacity,
    uint16_t *out_size,
    uint64_t *out_sender_tid);
uint16_t ipc_channel_count(void);

#endif
