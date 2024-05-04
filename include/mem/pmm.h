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

enum page_flags : uint8_t {
    PAGE_FLAGS_FREE = 1 
};

struct page {
    struct vnode* backing;

    uintmax_t offset;

    union {
        struct {
            struct page* free_next;
            struct page* free_prev;
        };
        struct {
            struct page* write_next;
            struct page* write_prev;
        };
    };

    uintmax_t refcount;
    enum page_flags flags;
};

extern uintptr_t hhdm_base;

void pmm_init(struct mmap* mmap);

void* pmm_alloc(size_t size, enum pmm_section_type section);
void* pmm_alloc_page(enum pmm_section_type section);

void pmm_release(void* vaddr);

#endif /* _AMETHYST_MEM_PMM_H */

