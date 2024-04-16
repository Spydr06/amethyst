#ifndef _AMETHYST_MEM_BITMAP_H
#define _AMETHYST_MEM_BITMAP_H

#include <stdint.h>
#include <stddef.h>

#include <mem/vmm.h>

#define BITMAP_ENTRY_FULL UINTPTR_MAX
#define BITMAP_ROW_BITS (sizeof(uintptr_t) * 8)

#define ADDRESS_TO_BITMAP_ENTRY(addr) ((addr) / PAGE_SIZE)

extern size_t memory_size_in_bytes;
extern uint32_t bitmap_size;
extern uint32_t used_frames;

enum address_type : uint8_t {
    ADDRESS_TYPE_PHYSICAL,
    ADDRESS_TYPE_VIRTUAL
};

extern uintptr_t* memory_map;

void bitmap_init(uintptr_t end_of_reserved_area);
void bitmap_get_region(uintptr_t* base_address, size_t* size, enum address_type type);

int64_t bitmap_request_frame(void);

uint32_t compute_kernel_entries(uintptr_t end_of_kernel_area);

inline void bitmap_set_bit(uint64_t location) {
    memory_map[location / BITMAP_ROW_BITS] |= 1 << (location % BITMAP_ROW_BITS);
}

inline void bitmap_set_bit_from_address(uintptr_t addr) {
    if(addr < memory_size_in_bytes)
        bitmap_set_bit(ADDRESS_TO_BITMAP_ENTRY(addr));
}

inline bool bitmap_test_bit(uint64_t location) {
    return memory_map[location / BITMAP_ROW_BITS] & (1 << (location % BITMAP_ROW_BITS));
}

#endif /* _AMETHYST_MEM_BITMAP_H */

