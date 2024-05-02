#ifndef _AMETHYST_X86_64_MMU_H
#define _AMETHYST_X86_64_MMU_H

#define PRESENT_BIT  0b00000001
#define WRITE_BIT    0b00000010
#define HUGEPAGE_BIT 0b10000000

#define PAGE_SIZE 0x1000

#define PRESENT_VIOLATION 0x1
#define WRITE_VIOLATION 0x2
#define ACCESS_VIOLATION 0x4
#define RESERVED_VIOLATION 0x8
#define FETCH_VIOLATION 0x10

#ifndef ASM_FILE

#include <cdefs.h>
#include <limine/limine.h>

void mmu_map_framebuffer(const struct limine_framebuffer* mmap);
__noreturn void page_fault_handler(uintptr_t error_code);

#endif /* ASM_FILE */

#endif /* _AMETHYST_X86_64_MMU_H */

