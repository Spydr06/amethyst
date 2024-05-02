#ifndef _AMETHYST_MEM_VMM_H
#define _AMETHYST_MEM_VMM_H

#include <stddef.h>
#include <stdint.h>

#ifdef __x86_64__
#include <x86_64/mem/mmu.h>
#define VMM_RESERVED_SPACE_SIZE 0x14480000000
#endif

enum paging_flags : uint64_t {
    VMM_FLAGS_NONE = 0,
    VMM_FLAGS_PRESENT = (1 << 0),
    VMM_FLAGS_WRITE_ENABLE = (1 << 1),
    VMM_FLAGS_USER_LEVEL = (1 << 2),
    VMM_FLAGS_ADDRESS_ONLY = (1 << 7),
    VMM_FLAGS_STACK = (1 << 8),
    VMM_FLAGS_NOEXEC = (1ull << 63),
};

enum vmm_level : uint8_t {
    VMM_LEVEL_USER,
    VMM_LEVEL_SUPERVISOR
};

struct vmm_info {

};

void vmm_init(enum vmm_level level, struct vmm_info* info);

void* vmm_alloc(size_t size, enum paging_flags flags, struct vmm_info* info);

void vmm_free(void* ptr, size_t size, enum paging_flags flags);

#endif /* _AMETHYST_MEM_VMM_H */

