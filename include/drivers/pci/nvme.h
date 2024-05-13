#ifndef _AMETHYST_DRIVERS_PCI_NVME_H
#define _AMETHYST_DRIVERS_PCI_NVME_H

#include <stdint.h>

enum nvme_admin_command : uint8_t {
    NVME_CREATE_SUBMISSION_QUEUE = 1,
    NVME_CREATE_COMPLETION_QUEUE = 5,
    NVME_IDENTIFY = 6,
};

enum nvme_io_command : uint8_t {
    NVME_READ = 2,
    NVME_WRITE = 1
};

struct nvme_command_dword {
    uint8_t opcode;
    uint8_t operation : 2;
    uint8_t __reserved : 4;
    uint8_t selection : 2;
    uint16_t identifier;
} __attribute__((packed));

static_assert(sizeof(struct nvme_command_dword) == sizeof(uint32_t));

struct nvme_submission_queue_entry {
    struct nvme_command_dword command;
    uint32_t namespace_id;
    uint64_t __reserved;
    uint64_t metadata_ptr;
    uint64_t data_ptr[2];
    uint32_t command_datap[6];
} __attribute__((packed));

static_assert(sizeof(struct nvme_submission_queue_entry) == 64);

struct nvme_completion_queue_entry {
    uint32_t command;
    uint32_t __reserved;
    uint16_t head_ptr;
    uint16_t queue_identifier;
    uint16_t command_identifier;
    uint8_t  phase_bit : 1;
    uint16_t status    : 15;
} __attribute__((packed));

static_assert(sizeof(struct nvme_completion_queue_entry) == 16);

#endif /* _AMETHYST_DRIVERS_PCI_NVME_H */

