#ifndef _AMETHYST_X86_64_MMU_H
#define _AMETHYST_X86_64_MMU_H

#include <cdefs.h>
#include <limine/limine.h>
#include <x86_64/cpu/cpu.h>

#include <mem/mmap.h>

#define PAGE_SIZE 0x1000

#define KERNELSPACE_START ((void*) 0xffff800000000000)
#define KERNELSPACE_END   ((void*) 0xffffffffffffffff)
#define USERSPACE_START   ((void*) 0x0000000000001000)
#define USERSPACE_END     ((void*) 0x0000800000000000)

enum mmu_violation {
    PRESENT_VIOLATION   = 0x1,
    WRITE_VIOLATION     = 0x2,
    ACCESS_VIOLATION    = 0x4,
    RESERVED_VIOLATION  = 0x8,
    FETCH_VIOLATION     = 0x10
};

enum mmu_flags : uint64_t {
    MMU_FLAGS_READ      = 1,
    MMU_FLAGS_WRITE     = 2,
    MMU_FLAGS_USER      = 4,
    MMU_FLAGS_NOEXEC    = 1lu << 63
};

typedef uint64_t* page_table_ptr_t;

void mmu_init(struct mmap* mmap);
void mmu_apswitch(void);

page_table_ptr_t mmu_new_table(void);
void mmu_destroy_table(page_table_ptr_t table);

bool mmu_map(page_table_ptr_t table, void* paddr, void* vaddr, enum mmu_flags flags);
void mmu_remap(page_table_ptr_t table, void* paddr, void* vaddr, enum mmu_flags flags);
void mmu_unmap(page_table_ptr_t table, void* vaddr);

void* mmu_get_physical(page_table_ptr_t table, void* vaddr);

bool mmu_get_flags(page_table_ptr_t table, void* vaddr, enum mmu_flags* mmu_flags);
bool mmu_is_present(page_table_ptr_t table, void* vaddr);
bool mmu_is_writable(page_table_ptr_t table, void* vaddr);

void mmu_tlb_shootdown(void* page);
void mmu_invalidate(void* vaddr);

__noreturn void page_fault_handler(struct cpu_context* status);

bool is_userspace_addr(const void* addr);

static __always_inline void mmu_switch(page_table_ptr_t table) {
    __asm__ volatile(
        "mov %%rax, %%cr3"
        ::"a"(table)
    );
}

#endif /* _AMETHYST_X86_64_MMU_H */

