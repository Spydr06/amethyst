#ifndef _AMETHYST_X86_64_PAGING_H
#define _AMETHYST_X86_64_PAGING_H

#define PRESENT_BIT  0b00000001
#define WRITE_BIT    0b00000010
#define HUGEPAGE_BIT 0b10000000

#define PAGE_SIZE 0x200000
#define PAGE_TABLE_ENTRY (HUGEPAGE_BIT | WRITE_BIT | PRESENT_BIT)

#define FRAMEBUFFER_MEM_START 0xffffffffbd000000

#define PD_ENTRY(addr)   (((addr) >> 21) & 0x1ff)
#define PDPR_ENTRY(addr) (((addr) >> 30) & 0x1ff)
#define PML4_ENTRY(addr) (((addr) >> 39) & 0x1ff)
#define PT_ENTRY(addr)   (((addr) >> 12) & 0x1ff)

#ifndef ASM_FILE

#include <stdint.h>

extern uint64_t p4_table[];
extern uint64_t p3_table[];
extern uint64_t p3_table_hh[];
extern uint64_t p2_table[];

void __framebuffer_map_page(const struct multiboot_tag_framebuffer* tag);

#endif /* ASM_FILE */

#endif /* _AMETHYST_X86_64_PAGING_H */

