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
void mmap_setup(void);
uintptr_t mmap_determine_bitmap_region(uint64_t lower_limit, size_t size);

#endif /* _AMETHYST_MEM_MMAP_H */

