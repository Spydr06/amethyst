#include <mem/bitmap.h>
#include <mem/mmap.h>
#include <mem/vmm.h>
#include <cdefs.h>
#include <kernelio.h>
#include <stdint.h>

#define ADDRESS_TO_BITMAP_ENTRY(addr) ((addr) / PAGE_SIZE)

extern char _end;

size_t memory_size_in_bytes = 0;
uint32_t bitmap_size = 0;
uint32_t used_frames;
static uint32_t number_of_entries = 0;
static uintptr_t memory_map_phys_addr;
static uintptr_t* memory_map = (uintptr_t*) &_end;

void bitmap_init(uintptr_t end_of_reserved_area) {
    bitmap_size = memory_size_in_bytes / PAGE_SIZE + 1;
    used_frames = 0;
    number_of_entries = bitmap_size / 64 + 1;
    memory_map_phys_addr = mmap_determine_bitmap_region(end_of_reserved_area, bitmap_size / 8 + 1); 

    uintptr_t end_of_mapped_physical_memory = (uintptr_t) ((void*) &end_of_mapped_memory) - (uintptr_t) _KERNEL_BASE_;
    if(memory_map_phys_addr > end_of_mapped_physical_memory)
        panic("unimplemented - Address %p is above the initially mapped memory: %p", (void*) memory_map_phys_addr, (void*) end_of_mapped_physical_memory);
    else
        klog(DEBUG, "Address %p is below the initially mapped memory: %p", (void*) memory_map_phys_addr, (void*) end_of_mapped_physical_memory);

    memory_map = (uintptr_t *) (memory_map_phys_addr + (uintptr_t) _KERNEL_BASE_);

    for(uint32_t i = 0; i < number_of_entries; i++)
        memory_map[i] = 0;

    uint32_t kernel_entries = compute_kernel_entries(end_of_reserved_area);
    uint32_t bitmap_rows = kernel_entries / 64;
    uint32_t i = 0;
    for(i = 0; i < bitmap_rows; i++)
        memory_map[i] = ~0;
    memory_map[i] = ~(~0ul << (kernel_entries - bitmap_rows * 64));
    used_frames = kernel_entries;

    klog(INFO, "Page size used by the kernel: %u", PAGE_SIZE);
    klog(DEBUG, "Physical memory size: %zu", memory_size_in_bytes);
    klog(DEBUG, "Memory bitmap has %u entries totalling %u bytes", number_of_entries, bitmap_size);
}

void bitmap_get_region(uintptr_t* base_address, size_t* size, enum address_type type) {
    switch(type) {
        case ADDRESS_TYPE_PHYSICAL:
            *base_address = memory_map_phys_addr;
            break;
        case ADDRESS_TYPE_VIRTUAL:
            *base_address = (uintptr_t) memory_map;
            break;
    }
    *size = bitmap_size / 8 + 1;
}

uint32_t compute_kernel_entries(uintptr_t end_of_kernel_area) {
    uint32_t kernel_entries = end_of_kernel_area / PAGE_SIZE + 1;
    if(end_of_kernel_area % PAGE_SIZE != 0)
        return kernel_entries + 1;
    return kernel_entries;
}

int64_t bitmap_request_frame(void) {
    uint16_t row, column;
    for(row = 0; row < number_of_entries; row++) {
        if(memory_map[row] != BITMAP_ENTRY_FULL) {
            for(column = 0; column < BITMAP_ROW_BITS; column++) {
                uintptr_t bit = 1 << column;
                if((memory_map[row] & bit) == 0) {
                    return row * BITMAP_ROW_BITS + column;
                }
            } 
        }
    }

    return -1;
}

void bitmap_set_bit(uint64_t location) {
    memory_map[location / BITMAP_ROW_BITS] |= 1 << (location % BITMAP_ROW_BITS);
}

void bitmap_set_bit_from_address(uintptr_t addr) {
    if(addr < memory_size_in_bytes)
        bitmap_set_bit(ADDRESS_TO_BITMAP_ENTRY(addr));
}
