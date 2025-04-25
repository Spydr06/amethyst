#ifndef _AMETHYST_MEM_PMM_H
#define _AMETHYST_MEM_PMM_H

#include <stddef.h>
#include <stdint.h>

#include <mem/mmap.h>

#define MAKE_HHDM(x) ((void*) (((uintptr_t) (x)) + hhdm_base))
#define FROM_HHDM(x) ((void*) (((uintptr_t) (x)) - hhdm_base))

enum pmm_section_type : int8_t {
    PMM_SECTION_1MB,
    PMM_SECTION_4GB,
    PMM_SECTION_DEFAULT,
    PMM_SECTION_COUNT
};

struct pmm_section {
    uintmax_t base_id;
    uintmax_t top_id;
    uintmax_t search_start;
};

extern uintptr_t hhdm_base;

#define pmm_alloc_page(section) (pmm_alloc(1, (section)))
#define pmm_free_page(addr) (pmm_free((addr), 1))

void pmm_init(struct mmap* mmap);

void* pmm_alloc(size_t size, enum pmm_section_type section);

void pmm_free(void* addr, size_t count);

uintmax_t pmm_total_memory(void);
uintmax_t pmm_free_memroy(void);

#endif /* _AMETHYST_MEM_PMM_H */

