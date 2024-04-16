#include "paging.h"

#include <arch/x86-common/cpu/cpu.h>
#include <kernelio.h>

__noreturn void page_fault_handler(uintptr_t error_code) {
    uint64_t cr2 = 0;
    __asm__ volatile (
        "mov %%cr2, %0"
        : "=r" (cr2)
    );

    uint64_t cr2_offset = cr2 & VM_OFFSET_MASK;
    uint64_t pd = PD_ENTRY(cr2_offset);
    uint64_t pdpr = PDPR_ENTRY(cr2_offset);
    uint64_t pml4 = PML4_ENTRY(cr2_offset);
    uint64_t *pd_table = (uint64_t*) (SIGN_EXTENSION | ENTRIES_TO_ADDRESS(510l, 510l, (uint64_t) pml4, (uint64_t) pdpr));
    uint64_t *pdpr_table = (uint64_t*) (SIGN_EXTENSION | ENTRIES_TO_ADDRESS(510l, 510l, 510l, (uint64_t) pml4));
    uint64_t *pml4_table = (uint64_t*) (SIGN_EXTENSION | ENTRIES_TO_ADDRESS(510l, 510l, 510l, 510l));

    panic(
        "Page Fault at address %p:\n"
        "    Error flags: FETCH(%lu) - RSVD(%lu) - ACCESS(%lu) - WRITE(%lu) - PRESENT(%lu)\n"
        "    Page Entries: pd: %p - pdpr: %p - pml4: %p\n"
        "                  pd  [%p] = %p\n"
        "                  pdpr[%p] = %p\n"
        "                  pml4[%p] = %p",
        (void*) cr2,
        error_code & FETCH_VIOLATION,
        error_code & RESERVED_VIOLATION,
        error_code & ACCESS_VIOLATION,
        error_code & WRITE_VIOLATION,
        error_code & PRESENT_VIOLATION,
        (void*) pd, (void*) pdpr, (void*) pml4,
        (void*) pd, (void*) pd_table[pd],
        (void*) pdpr, (void*) pdpr_table[pdpr],
        (void*) pml4, (void*) pml4_table[pml4]
    );
}
