#include "kernelio.h"
#include "scheduling/thread.h"
#include <mem/vmm.h>
#include <mem/pmm.h>

struct vmm_info vmm_kernel;

void* vmm_alloc(size_t size, enum paging_flags flags, struct vmm_info* info);
size_t align_value_to_page(size_t value);
bool is_address_aligned(size_t value, size_t align);

static size_t parse_flags(size_t flags) {
    return flags & ~(1 << 7);
}

void* vmm_alloc_at(uintptr_t base_address, size_t size, enum paging_flags flags, struct vmm_info* info) {
    if(!info)
        info = &vmm_kernel;

    if(!size)
        return nullptr;
    
    if(info->status.cur_index >= info->status.items_per_page) {
        void* new_container_phys_address = pmm_alloc_frame();
        if(new_container_phys_address) {    
            struct vmm_container* new_container = (struct vmm_container*) align_value_to_page((uint64_t) info->status.cur_container + sizeof(struct vmm_container) + PAGE_SIZE);
            klog(DEBUG, "New address %p is aligned: %d.", new_container, is_address_aligned((uintptr_t) new_container, PAGE_SIZE));

            map_phys_to_virt_addr_hh(new_container_phys_address, new_container, VMM_FLAGS_PRESENT | VMM_FLAGS_WRITE_ENABLE, (uint64_t*) info->root_table_hddm);
            info->status.cur_index = 0;
            new_container->next = nullptr;
            info->status.cur_container->next = new_container;
            info->status.cur_container = new_container;
        }
        else {
            klog(ERROR, "pmm_alloc_frame() returned NULL.");
            return nullptr;
        }
    }

    size_t new_size = align_value_to_page(size);
    uintptr_t return_address = info->status.next_available_address;
    if(base_address != 0 && base_address > return_address) {
        if(!is_address_aligned(base_address, PAGE_SIZE))
            klog(ERROR, "Base address %p is not aligned to 0x%x", (void*) base_address, PAGE_SIZE);
        klog(DEBUG, "Allocating address %p", (void*) base_address);
        info->status.next_available_address = base_address;
        return_address = base_address;
    }

    info->status.cur_container->root[info->status.cur_index].base = return_address;
    info->status.cur_container->root[info->status.cur_index].flags = flags;
    info->status.cur_container->root[info->status.cur_index].size = new_size;

    if(!IS_ADDRESS_HIGHER_HALF(return_address))
        flags |= VMM_FLAGS_USER_LEVEL;

    klog(DEBUG, "Flags PRESENT(%lu) - WRITE(%lu) - USER(%lu)", flags & VMM_FLAGS_PRESENT, flags & VMM_FLAGS_WRITE_ENABLE, flags & VMM_FLAGS_USER_LEVEL);

    if(!(flags & VMM_FLAGS_ADDRESS_ONLY)) {
        size_t required_pages = align_value_to_page(size) / PAGE_SIZE;
        size_t arch_flags = parse_flags(flags);

        klog(DEBUG, "No physical memory needed: mapping address %p", (void*) info->root_table_hddm);

        for(size_t i = 0; i < required_pages; i++) {
            void* frame = pmm_alloc_frame();
            klog(DEBUG, "Address to map: %p - physical frame: %p", frame, (void*) return_address);
            map_phys_to_virt_addr_hh(
                (void*) frame,
                (void*) return_address + i * PAGE_SIZE,
                arch_flags | VMM_FLAGS_PRESENT | VMM_FLAGS_WRITE_ENABLE,
                (uint64_t*) info->root_table_hddm
            );
        }
    }

    info->status.cur_index++;

    return flags & VMM_FLAGS_STACK
        ? ((void*) return_address + THREAD_DEFAULT_STACK_SIZE) 
        : (void*) return_address;
}

void vmm_free(void* ptr, size_t size, enum paging_flags flags) {
    klog(WARN, "vmm_free() not implmented!");
}

static void* map_vaddress(void* virtual_address, enum paging_flags flags, uint64_t* pml4_root) {
    void* new_addr = pmm_alloc_frame();
    return map_phys_to_virt_addr_hh(new_addr, virtual_address, flags, pml4_root);
}

void map_vaddress_range(void* virtual_address, enum paging_flags flags, size_t required_pages, uint64_t* pml4_root) {
    for(size_t i = 0; i < required_pages; i++)
        map_vaddress(virtual_address + (i * PAGE_SIZE), flags, pml4_root);
}

