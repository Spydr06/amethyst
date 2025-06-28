#ifndef _AMETHYST_DRIVERS_PCI_NVME_H
#define _AMETHYST_DRIVERS_PCI_NVME_H

#include "sys/spinlock.h"
#include <stdint.h>

#define PCI_PROG_IF_NVME 0x02

enum nvme_capabilities : uint64_t {
    NVME_CAP_MPSMAX_OFF = 52,
    NVME_CAP_MPSMIN_OFF = 48,

    NVME_CAP_MPSMAX = 0x0ful << NVME_CAP_MPSMAX_OFF,
    NVME_CAP_MPSMIN = 0x0ful << NVME_CAP_MPSMIN_OFF,
};

enum nvme_admin_command : uint8_t {
    NVME_CREATE_SUBMISSION_QUEUE = 1,
    NVME_CREATE_COMPLETION_QUEUE = 5,
    NVME_IDENTIFY = 6,
};

enum nvme_io_command : uint8_t {
    NVME_READ = 2,
    NVME_WRITE = 1
};

struct nvme_command {
    uint8_t opcode;
    uint8_t operation : 2;
    uint8_t __reserved : 4;
    uint8_t selection : 2;
    uint16_t identifier;
} __attribute__((packed));

static_assert(sizeof(struct nvme_command) == sizeof(uint32_t));

struct nvme_submission_queue_entry {
    struct nvme_command command;
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

struct nvme_base_registers {
    uint64_t cap; // capabilites
    uint32_t vs; // version 
    uint32_t intms; // interrupt mask set
    uint32_t intmc; // interrupt mask clear
    uint16_t cc; // controller configuration
    uint64_t __reserved0 : 48;
    uint16_t csts; // controller status
    uint64_t __reserved1 : 48;
    uint32_t aqa; // admin queue attributes
    uint64_t asq; // admin submission queue
    uint64_t acq; // admin completion queue
} __attribute__((packed));

static_assert(sizeof(struct nvme_base_registers) == 0x38);

struct nvme_version {
    uint8_t tertiary;
    uint8_t minor;
    uint16_t major;
} __attribute__((packed));

static_assert(sizeof(struct nvme_version) == sizeof(uint32_t));

struct nvme_device {
    spinlock_t device_lock;

    union {
        void* mmio_addr;
        struct nvme_base_registers* base_registers;
    };

    uint64_t base_addr;
    uint8_t capability_stride;
};

static inline uint64_t nvme_get_max_pagesize(uint64_t cap) {
    uint64_t mpsmax = (cap & NVME_CAP_MPSMAX) >> NVME_CAP_MPSMAX_OFF;
    return 2ul << (11ul + mpsmax); // 2 ^ (12 + mpsmax)
}

static inline uint64_t nvme_get_min_pagesize(uint64_t cap) {
    uint64_t mpsmin = (cap & NVME_CAP_MPSMIN) >> NVME_CAP_MPSMIN_OFF;
    return 2ul << (11ul + mpsmin); // 2 ^ (12 + mpsmin)
}

static inline struct nvme_version nvme_get_version(uint32_t ver) {
    union {
        uint32_t a;
        struct nvme_version b;
    } u;
    u.a = ver;
    return u.b;
}

void nvme_init(void);

#endif /* _AMETHYST_DRIVERS_PCI_NVME_H */

