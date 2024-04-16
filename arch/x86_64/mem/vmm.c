#include <mem/vmm.h>
#include <mem/pmm.h>
#include <stdint.h>
#include "kernelio.h"
#include "mem/bitmap.h"
#include "mem/mmap.h"
#include "paging.h"

struct vmm_container* vmm_container_root = nullptr;

void vmm_init(enum vmm_level level, struct vmm_info* info) {
    uint64_t* root_table_hh;

    if(info) {
        klog(DEBUG, "Task vmm initialization: root_table_hddm: %p", (void*) info->root_table_hddm);
        root_table_hh = (uint64_t*) info->root_table_hddm;
    }
    else {
        klog(DEBUG, "Kernel vmm initialization");
        info = &vmm_kernel;
        root_table_hh = (uint64_t*) paging_status.hhdm_page_root_address;
    }

    info->vmm_data_start = align_value_to_page(higher_half_direct_map_base + memory_size_in_bytes + PAGE_SIZE /* padding */);
    info->status.container_root = (struct vmm_container*) info->vmm_data_start;
    info->status.vmm_data_end = (uint64_t) vmm_container_root + VMM_RESERVED_SPACE_SIZE;

    switch(level) {
        case VMM_LEVEL_SUPERVISOR:
            klog(DEBUG, "Supervisor level initialization");
            info->vmm_space_start = info->vmm_data_start + VMM_RESERVED_SPACE_SIZE + PAGE_SIZE /* padding */;
            info->start_of_vmm_space = (size_t) info->status.container_root + VMM_RESERVED_SPACE_SIZE + PAGE_SIZE /* padding */;
            break;
        case VMM_LEVEL_USER:
            klog(DEBUG, "User level initialization");
            info->vmm_space_start = PAGE_SIZE /* padding */;
            info->start_of_vmm_space = PAGE_SIZE /* padding */;
            break;
        default:
            panic("Unsupported vmm privilege level %hhu.", level);
    }

    info->status.next_available_address = info->start_of_vmm_space;
    info->status.items_per_page = PAGE_SIZE - sizeof(struct vmm_item) - 1;
    info->status.cur_index = 0;

#ifndef NDEBUG
    klog(DEBUG, "\tvmm_container_root starts at: %p - %p", (void*) info->status.container_root, (void*) is_address_aligned(info->vmm_data_start, PAGE_SIZE));
    klog(DEBUG, "\tvmmDataStart  starts at: %p - %p (end_of_vmm_data)", (void*) info->vmm_data_start, (void*) info->status.vmm_data_end);
    klog(DEBUG, "\thigherHalfDirectMapBase: %p - %p", (void*) higher_half_direct_map_base, (void*) is_address_aligned(higher_half_direct_map_base, PAGE_SIZE));
    klog(DEBUG, "\tvmmSpaceStart: %p - start_of_vmm_space: (%p)", (void*) info->vmm_space_start, (void*) info->start_of_vmm_space);
    klog(DEBUG, "\tsizeof struct vmm_container: 0x%lu", sizeof(struct vmm_container));
#endif

    void* vmm_root_phys = pmm_alloc_frame();
    if(!vmm_root_phys) {
        klog(ERROR, "%s: vmm_root_phys should not be NULL.", __func__);
        return;
    }

    map_phys_to_virt_addr_hh(vmm_root_phys, info->status.container_root, VMM_FLAGS_PRESENT | VMM_FLAGS_WRITE_ENABLE, root_table_hh);
    info->status.container_root->next = nullptr;
    info->status.cur_container = info->status.container_root;
}

void* map_phys_to_virt_addr(void* physical_addr, void* address, enum paging_flags flags) {
    uint16_t pml4_e = PML4_ENTRY((uintptr_t) address);
    uint16_t pdpr_e = PDPR_ENTRY((uintptr_t) address);
    uint16_t pd_e = PD_ENTRY((uintptr_t) address);
    
    uint8_t user_mode_status = 0;
    if(!IS_ADDRESS_HIGHER_HALF((uint64_t) address)) {
        flags |= VMM_FLAGS_USER_LEVEL;
        user_mode_status = VMM_FLAGS_USER_LEVEL;
    }

    uint64_t* pml4_table = (uint64_t*) (SIGN_EXTENSION | ENTRIES_TO_ADDRESS(510l, 510l, 510l, 510l));
    uint64_t* pdpr_table = (uint64_t*) (SIGN_EXTENSION | ENTRIES_TO_ADDRESS(510l, 510l, 510l, (uint64_t) pml4_e));
    uint64_t* pd_table = (uint64_t*) (SIGN_EXTENSION | ENTRIES_TO_ADDRESS(510l, 510l, (uint64_t) pml4_e, (uint64_t) pdpr_e));

    if(!(pml4_table[pml4_e] & 0b1)) {
        uint64_t* new_table = pmm_alloc_frame();
        pml4_table[pml4_e] = (uint64_t) new_table | user_mode_status | WRITE_BIT | PRESENT_BIT;
        clean_new_table(pdpr_table);
    }

    if(!(pdpr_table[pdpr_e] & 0b1)) {
        uint64_t* new_table = pmm_alloc_frame();
        pdpr_table[pdpr_e] = (uint64_t) new_table | user_mode_status | WRITE_BIT | PRESENT_BIT;
        clean_new_table(pd_table);
    }

    if(!(pd_table[pd_e] & 0b1)) {
        pd_table[pd_e] = (uint64_t) physical_addr | HUGEPAGE_BIT | flags | user_mode_status;
    }

    return address;
}

void* map_phys_to_virt_addr_hh(void* physical_address, void* address, enum paging_flags flags, uint64_t* pml4_root) {
    uint16_t pml4_e = PML4_ENTRY((uintptr_t) address);
    uint16_t pdpr_e = PDPR_ENTRY((uintptr_t) address);
    uint16_t pd_e = PD_ENTRY((uintptr_t) address);

    uint8_t user_mode_status = 0;

    if(!IS_ADDRESS_HIGHER_HALF((uint64_t) address)) {
        flags |= VMM_FLAGS_USER_LEVEL;
        user_mode_status = VMM_FLAGS_USER_LEVEL;
    }

    if(!pml4_root)
        pml4_root = paging_status.hhdm_page_root_address;

    if(!pml4_root)
        return nullptr;

#ifndef NDEBUG
    klog(DEBUG, "Entries values pml4_e: 0x%04hx pdpr_e: 0x%04hx pd_e: 0x%04hx", pml4_e, pdpr_e, pd_e);
    klog(DEBUG, "\taddress: %p, phys_address: %p", address, physical_address);
    klog(DEBUG, "\tpdpr base_address: %p", (void*) (pml4_root[pml4_e] & VM_PAGE_TABLE_BASE_ADDRESS_MASK));
#endif

//    uint64_t *pml4_table = pml4_root;
    uint64_t *pdpr_root = nullptr;
    uint64_t *pd_root = nullptr;

    if(!(pml4_root[pml4_e] & 1)) {
        klog(DEBUG, "Allocating new table at pml4_e = 0x%04hx", pml4_e);
        uint64_t* new_table = pmm_alloc_frame();
        pml4_root[pml4_e] = (uint64_t) new_table | user_mode_status | WRITE_BIT | PRESENT_BIT;
        uint64_t* new_table_hhdm = hhdm_get_variable((uintptr_t) new_table);
        clean_new_table(new_table_hhdm);
        pdpr_root = new_table_hhdm;
    }
    else {
        klog(DEBUG, "pml4 already allocated.");
        pdpr_root = (uint64_t*) hhdm_get_variable((uintptr_t) pml4_root[pml4_e] & VM_PAGE_TABLE_BASE_ADDRESS_MASK);
    }

    if(!(pdpr_root[pdpr_e] & 1)) {
        klog(DEBUG, "Allocating new table at pdpr_e = 0x%04hx", pdpr_e);
        uint64_t* new_talbe = pmm_alloc_frame();
        pdpr_root[pdpr_e] = (uint64_t) new_talbe | user_mode_status | WRITE_BIT | PRESENT_BIT;
        uint64_t* new_table_hhdm = hhdm_get_variable((uintptr_t) new_talbe);
        clean_new_table(new_table_hhdm);
        pd_root = new_table_hhdm;
    }
    else {
        klog(DEBUG, "pdpr already allocated.");
        pd_root = (uint64_t*) hhdm_get_variable((uintptr_t) pdpr_root[pdpr_e] & VM_PAGE_TABLE_BASE_ADDRESS_MASK);
    }

    if(!(pd_root[pd_e] & 1)) {
        pd_root[pd_e] = (uint64_t) physical_address | HUGEPAGE_BIT | flags | user_mode_status;
        klog(DEBUG, "PD Flags: 0x%hx entry value pd_root[0x%04hx] = %p", flags, pd_e, (void*) pd_root[pd_e]);
    }

    return address;
}

enum phys_address is_physical_address_mapped(uintptr_t physical_address, uintptr_t virtual_address) {
    uint16_t pml4_e = PML4_ENTRY((uint64_t) virtual_address);
    uint64_t *pml4_table = (uint64_t *) (SIGN_EXTENSION | ENTRIES_TO_ADDRESS(510l, 510l, 510l, 510l));
    if (!(pml4_table[pml4_e] & PRESENT_BIT))
        return PHYS_ADDRESS_NOT_MAPPED;

    uint16_t pdpr_e = PDPR_ENTRY((uint64_t) virtual_address);
    uint64_t *pdpr_table = (uint64_t *) (SIGN_EXTENSION | ENTRIES_TO_ADDRESS(510l, 510l, 510l, (uint64_t)  pml4_e));
    if (!(pdpr_table[pdpr_e] & PRESENT_BIT))
        return PHYS_ADDRESS_NOT_MAPPED;

    uint16_t pd_e = PD_ENTRY((uint64_t) virtual_address);
    uint64_t *pd_table = (uint64_t*) (SIGN_EXTENSION | ENTRIES_TO_ADDRESS(510l, 510l, pml4_e, (uint64_t)  pdpr_e));
    if (!(pd_table[pd_e] & PRESENT_BIT))
        return PHYS_ADDRESS_NOT_MAPPED;
    else if (ALIGN_PHYSADDRESS(pd_table[pd_e]) == ALIGN_PHYSADDRESS(physical_address))
        return PHYS_ADDRESS_MAPPED; 
    else
        return PHYS_ADDRESS_MISMATCH;

    return PHYS_ADDRESS_NOT_MAPPED; 
}

