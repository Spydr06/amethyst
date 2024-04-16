#ifndef _AMETHYST_X86_64_PAGING_H
#define _AMETHYST_X86_64_PAGING_H

#define PRESENT_BIT  0b00000001
#define WRITE_BIT    0b00000010
#define HUGEPAGE_BIT 0b10000000

#define PAGE_SIZE 0x200000
#define PAGE_TABLE_ENTRY (HUGEPAGE_BIT | WRITE_BIT | PRESENT_BIT)
#define PAGE_ALIGNMENT_MASK (PAGE_SIZE - 1)

#define PAGES_PER_TABLE 0x200

#define ALIGN_PHYSADDRESS(addr) (addr & (~PAGE_ALIGNMENT_MASK))

#define FRAMEBUFFER_MEM_START 0xffffffffbd000000
#define HIGHER_HALF_ADDRESS_OFFSET 0xFFFF800000000000
#define VM_OFFSET_MASK 0xFFFFFFFFFFE00000
#define SIGN_EXTENSION 0xFFFF000000000000

#define PD_ENTRY(addr)   (((addr) >> 21) & 0x1ff)
#define PDPR_ENTRY(addr) (((addr) >> 30) & 0x1ff)
#define PML4_ENTRY(addr) (((addr) >> 39) & 0x1ff)
#define PT_ENTRY(addr)   (((addr) >> 12) & 0x1ff)

#define ENTRIES_TO_ADDRESS(pml4, pdpr, pd, pt) ((pml4 << 39) | (pdpr << 30) | (pd << 21) |  (pt << 12))

#define PRESENT_VIOLATION 0x1
#define WRITE_VIOLATION 0x2
#define ACCESS_VIOLATION 0x4
#define RESERVED_VIOLATION 0x8
#define FETCH_VIOLATION 0x10


#ifndef ASM_FILE

#include <stdint.h>
#include <cdefs.h>

#define IS_ADDRESS_HIGHER_HALF(addr) ((addr) & (1lu << 62))
#define ENSURE_HIGHER_HALF(addr) ((addr) > HIGHER_HALF_ADDRESS_OFFSET ? (addr) : (addr) + HIGHER_HALF_ADDRESS_OFFSET)

extern uint64_t p4_table[];
extern uint64_t p3_table[];
extern uint64_t p3_table_hh[];
extern uint64_t p2_table[];

void __framebuffer_map_page(const struct multiboot_tag_framebuffer* tag);

static __always_inline void clean_new_table(uint64_t* table) {
    for(unsigned i = 0; i < PAGES_PER_TABLE; i++)
        table[i] = 0l;
}

__noreturn void page_fault_handler(uintptr_t error_code);

#endif /* ASM_FILE */

#endif /* _AMETHYST_X86_64_PAGING_H */

