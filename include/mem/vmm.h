#ifndef _AMETHYST_MEM_VMM_H
#define _AMETHYST_MEM_VMM_H

#include <stddef.h>
#include <stdint.h>

#ifdef __x86_64__
#include <arch/x86_64/mem/paging.h>
#endif

enum paging_flags : uint16_t {
    VMM_FLAGS_NONE = 0,
    VMM_FLAGS_PRESENT = (1 << 0),
    VMM_FLAGS_WRITE_ENABLE = (1 << 1),
    VMM_FLAGS_USER_LEVEL = (1 << 2),
    VMM_FLAGS_ADDRESS_ONLY = (1 << 7),
    VMM_FLAGS_STACK = (1 << 8)
};

enum vmm_level : uint8_t {
    VMM_LEVEL_USER,
    VMM_LEVEL_SUPERVISOR
};

void* map_phys_to_virt_addr(void* physical_address, void* address, size_t flags);

#endif /* _AMETHYST_MEM_VMM_H */

