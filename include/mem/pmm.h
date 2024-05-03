#ifndef _AMETHYST_MEM_PMM_H
#define _AMETHYST_MEM_PMM_H

#include <stddef.h>
#include <stdint.h>

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

    struct page* hash_next;
    struct page* hash_prev;
    struct page* vnode_next;
    struct page* vnode_prev;

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
} __attribute__((packed));

extern uintptr_t hhdm_base;

void pmm_init(void);

void* pmm_alloc(size_t size, enum pmm_section_type section);
void* pmm_alloc_page(enum pmm_section_type section);

#endif /* _AMETHYST_MEM_PMM_H */

