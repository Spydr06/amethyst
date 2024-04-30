#ifndef _AMETHYST_MEM_VMM_H
#define _AMETHYST_MEM_VMM_H

#include <stddef.h>
#include <stdint.h>

#ifdef __x86_64__
#include <x86_64/mem/paging.h>
#define VMM_RESERVED_SPACE_SIZE 0x14480000000
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

enum phys_address : uint8_t {
    PHYS_ADDRESS_NOT_MAPPED = 0,
    PHYS_ADDRESS_MAPPED = 1,
    PHYS_ADDRESS_MISMATCH = 2
};

struct vmm_item {
    uintptr_t base;
    size_t size;
    enum paging_flags flags;
};

struct vmm_container {
    struct vmm_item root[PAGE_SIZE / sizeof(struct vmm_item) - 1];
    struct vmm_container* next;
} __attribute__((packed));

struct vmm_info {
    uintptr_t vmm_data_start;
    uintptr_t vmm_space_start;

    size_t start_of_vmm_space;
    uintptr_t root_table_hddm;

    struct vmm_status {
        size_t items_per_page;
        size_t cur_index;
        size_t next_available_address;
        uintptr_t vmm_data_end;

        struct vmm_container* container_root;
        struct vmm_container* cur_container;
    } status;
};

extern struct vmm_info vmm_kernel;

// implemented in arch/<arch>/mem/vmm.c
void* map_phys_to_virt_addr(void* physical_address, void* address, enum paging_flags flags);
void* map_phys_to_virt_addr_hh(void* physical_addres, void* address, enum paging_flags flags, uint64_t* pml4_root);

void map_vaddress_range(void* virtual_address, enum paging_flags flags, size_t required_pages, uint64_t* pml4_root);

void vmm_init(enum vmm_level level, struct vmm_info* info);
enum phys_address is_physical_address_mapped(uintptr_t physicall_address, uintptr_t virtual_address);

// implemented in kernel/mem/vmm.c
void* vmm_alloc_at(uintptr_t base_address, size_t size, enum paging_flags flags, struct vmm_info* info);

inline void* vmm_alloc(size_t size, enum paging_flags flags, struct vmm_info* info) {
    return vmm_alloc_at(0x0, size, flags, info);
}

inline size_t align_value_to_page(size_t value) {
    return ((value + PAGE_SIZE - 1) / PAGE_SIZE) * PAGE_SIZE;
}

inline bool is_address_aligned(size_t value, size_t align) {
    return value % align == 0;
}

#endif /* _AMETHYST_MEM_VMM_H */

