#include <mem/mmap.h>
#include <mem/vmm.h>
#include "cdefs.h"
#include "kernelio.h"
#include "mem/bitmap.h"
#include <x86_64/mem/mmu.h>

uintptr_t higher_half_direct_map_base;

void hhdm_map_physical_memory(void) {
    uint64_t end_of_mapped_physical_memory = end_of_mapped_memory - (uintptr_t) _KERNEL_BASE_;
    if(is_physical_address_mapped(end_of_mapped_physical_memory, end_of_mapped_physical_memory)) {
        end_of_mapped_memory += PAGE_SIZE;
        end_of_mapped_physical_memory += PAGE_SIZE;
    }

    uint64_t address_to_map = 0;
    uint64_t virtual_address = 
        higher_half_direct_map_base = (uint64_t) HIGHER_HALF_ADDRESS_OFFSET + PAGE_SIZE /* padding */;

    klog(DEBUG, "Higher-half initial entries: pml4: %lu, pdpr: %lu, pd: %lu", 
        PML4_ENTRY((uint64_t) higher_half_direct_map_base),
        PDPR_ENTRY((uint64_t) higher_half_direct_map_base),
        PD_ENTRY((uint64_t) higher_half_direct_map_base)
    );

    size_t current_pml4_entry = PML4_ENTRY((uint64_t) higher_half_direct_map_base);

    if(!(p4_table[current_pml4_entry] & 1))
        panic("p4_table[%zu] & 1 == 0 // this shouldn't happen.", current_pml4_entry);

    while(address_to_map < memory_size_in_bytes) {
        map_phys_to_virt_addr((void*) address_to_map, (void*) virtual_address, VMM_FLAGS_PRESENT | VMM_FLAGS_WRITE_ENABLE);
        address_to_map += PAGE_SIZE;
        virtual_address += PAGE_SIZE;
    }

    klog(DEBUG, "Physical memory mapped end: %p - virtual memory direct end: %p - counter: %zu", 
        (void*) end_of_mapped_physical_memory,
        (void*) end_of_mapped_memory,
        current_pml4_entry
    );

    paging_status.page_root_address = p4_table;
    uint64_t p4_table_phys_address = (uint64_t) p4_table - (uintptr_t) _KERNEL_BASE_;
    paging_status.hhdm_page_root_address = (uint64_t*) hhdm_get_variable((uintptr_t) p4_table_phys_address);
}

void* hhdm_get_variable(uintptr_t phys_address) {
    if(phys_address < memory_size_in_bytes) {
        klog(DEBUG, "Physical address %p -> returning %p", (void*) phys_address, (void*) phys_address + higher_half_direct_map_base);
        return (void*) phys_address + higher_half_direct_map_base;
    }

    klog(ERROR, "Address %p is not in physical memory.", (void*) phys_address);
    return nullptr;
}

