#ifndef _AMETHYST_MEM_MMAP_H
#define _AMETHYST_MEM_MMAP_H

#include <stdint.h>
#include <stddef.h>
#include <cdefs.h>

#include <limine/limine.h>

enum mmap_type : uint8_t {
    MMAP_AVAILABLE,
    MMAP_RESERVED,
    MMAP_ACPI_RECLAIMABLE,
    MMAP_ACPI_NVS,
    MMAP_BAD_MEMORY,
    MMAP_BOOTLOADER_RECLAIMABLE,
    MMAP_KERNEL_AND_MODULES,
    MMAP_FRAMEBUFFER
};

struct mmap {
    struct limine_memmap_response* mmap;
    struct limine_memmap_entry* biggest_entry;
    size_t memory_size;
    size_t top;
};

int mmap_parse(struct mmap* mmap);
const char* mmap_strtype(enum mmap_type type);

#endif /* _AMETHYST_MEM_MMAP_H */

