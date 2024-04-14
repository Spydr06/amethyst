#include <mem/vmm.h>
#include <mem/pmm.h>
#include <stdint.h>
#include "paging.h"

void* map_phys_to_virt_addr(void* physical_addr, void* address, size_t flags) {
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

