#ifndef _AMETHYST_MEM_MMAP_H
#define _AMETHYST_MEM_MMAP_H

#include "multiboot2.h"
#include <stdint.h>
#include <stddef.h>

enum mmap_state : uint8_t {
    MMAP_INVALID = 0,
    MMAP_AVAILABLE,
    MMAP_RESERVED,
    MMAP_RECLAIMABLE,
    MMAP_NVS,
    MMAP_DEFECTIVE
};

extern const char* mmap_types[];
extern uintptr_t end_of_mapped_memory;

void mmap_parse(const struct multiboot_tag_mmap* mmap_root);
void mmap_setup(const struct multiboot_tag_basic_meminfo* mmap_root);
uintptr_t mmap_determine_bitmap_region(uint64_t lower_limit, size_t size);

// platform-specific
void hhdm_map_physical_memory(void);
void* hhdm_get_variable(uintptr_t phys_address);

#endif /* _AMETHYST_MEM_MMAP_H */

