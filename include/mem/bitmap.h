#ifndef _AMETHYST_MEM_BITMAP_H
#define _AMETHYST_MEM_BITMAP_H

#include <stdint.h>
#include <stddef.h>

extern size_t memory_size_in_bytes;

enum address_type : uint8_t {
    ADDRESS_TYPE_PHYSICAL,
    ADDRESS_TYPE_VIRTUAL
};

void bitmap_init(uintptr_t end_of_reserved_area);
void bitmap_get_region(uintptr_t* base_address, size_t* size, enum address_type type);

uint32_t compute_kernel_entries(uintptr_t end_of_kernel_area);

#endif /* _AMETHYST_MEM_BITMAP_H */

