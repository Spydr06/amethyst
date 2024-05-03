#include <x86_64/mem/mmu.h>
#include <kernelio.h>

__noreturn void page_fault_handler(uintptr_t error_code) {
    uint64_t cr2 = 0;
    __asm__ volatile (
        "mov %%cr2, %0"
        : "=r" (cr2)
    );

    /*uint64_t cr2_offset = cr2 & VM_OFFSET_MASK;
    uint64_t pd = PD_ENTRY(cr2_offset);
    uint64_t pdpr = PDPR_ENTRY(cr2_offset);
    uint64_t pml4 = PML4_ENTRY(cr2_offset);
    uint64_t *pd_table = (uint64_t*) (SIGN_EXTENSION | ENTRIES_TO_ADDRESS(510l, 510l, (uint64_t) pml4, (uint64_t) pdpr));
    uint64_t *pdpr_table = (uint64_t*) (SIGN_EXTENSION | ENTRIES_TO_ADDRESS(510l, 510l, 510l, (uint64_t) pml4));
    uint64_t *pml4_table = (uint64_t*) (SIGN_EXTENSION | ENTRIES_TO_ADDRESS(510l, 510l, 510l, 510l));*/

    panic(
        "Page Fault at address %p:\n"
        "    Error flags: FETCH(%lu) - RSVD(%lu) - ACCESS(%lu) - WRITE(%lu) - PRESENT(%lu)\n"
      /*  "    Page Entries: pd: %p - pdpr: %p - pml4: %p\n"
        "                  pd  [%p] = %p\n"
        "                  pdpr[%p] = %p\n"
        "                  pml4[%p] = %p"*/,
        (void*) cr2,
        error_code & FETCH_VIOLATION,
        error_code & RESERVED_VIOLATION,
        error_code & ACCESS_VIOLATION,
        error_code & WRITE_VIOLATION,
        error_code & PRESENT_VIOLATION/*,
        (void*) pd, (void*) pdpr, (void*) pml4,
        (void*) pd, (void*) pd_table[pd],
        (void*) pdpr, (void*) pdpr_table[pdpr],
        (void*) pml4, (void*) pml4_table[pml4]*/
    );
}

void mmu_map_framebuffer(const struct limine_framebuffer* mmap) {
    /*uint64_t phys_addr = (uint64_t) tag->common.framebuffer_addr;

    phys_addr |= PAGE_TABLE_ENTRY;
    *(uint64_t*) (((void*) p2_table) + 8 * 488) = phys_addr;
    phys_addr += PAGE_SIZE;
    phys_addr |= PAGE_TABLE_ENTRY;
    *(uint64_t*) (((void*) p2_table) + 8 * 489) = phys_addr;

    phys_addr = (uint64_t) tag->common.framebuffer_addr;

    uint32_t mem_size = (tag->common.framebuffer_pitch * tag->common.framebuffer_height); 

    uint32_t entries = mem_size / PAGE_SIZE;
    uint32_t entries_mod = mem_size % PAGE_SIZE;
    
    uint32_t pd = PD_ENTRY(FRAMEBUFFER_MEM_START);
    uint32_t pdpr = PDPR_ENTRY(FRAMEBUFFER_MEM_START);
    uint32_t pml4 = PML4_ENTRY(FRAMEBUFFER_MEM_START);

    if(p4_table[pml4] == 0 && p3_table_hh[pdpr] == 0)
        panic("PML4 or PDPR empty - could not initialize framebuffer.");
    
    if(entries_mod != 0)
        entries++;

    for(size_t i = 0; entries > 0; i++) {
        entries--;
        if((p2_table[pd + i] < phys_addr ||
            p2_table[pd + i] > (phys_addr + mem_size)) ||
            p2_table[pd + i] == 0) {
                p2_table[pd + i] = (phys_addr + (i * PAGE_SIZE)) | PAGE_TABLE_ENTRY;
        }
    }*/
    unimplemented();
}

