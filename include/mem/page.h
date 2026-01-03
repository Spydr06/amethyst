#ifndef _AMETHYST_MEM_PAGE_H
#define _AMETHYST_MEM_PAGE_H

#include <stdint.h>
#include <kernelio.h>
#include <mem/slab.h>
#include <mem/pmm.h>

enum page_flags : uint8_t {
    PAGE_FLAGS_FREE      = 1,
    PAGE_FLAGS_DIRTY     = 2,
    PAGE_FLAGS_TRUNCATED = 4,
    PAGE_FLAGS_ERROR     = 8,
    PAGE_FLAGS_READY     = 16,
    PAGE_FLAGS_PINNED    = 32,
};

struct page {
    void* physical_addr;
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
};

extern struct scache* page_meta_cache; // declared in vmm.c

static inline void page_hold(struct page* page) {
    __atomic_add_fetch(&page->refcount, 1, __ATOMIC_SEQ_CST);
}

static inline void page_release(struct page* page) {
    __atomic_sub_fetch(&page->refcount, 1, __ATOMIC_SEQ_CST);

    // free page if not refrenced
    if(page->refcount == 0) {
        klog(INFO, "free page");
        slab_free(page_meta_cache, page);
        pmm_free_page(page->physical_addr);
    }
}

static inline void* page_get_physical(struct page* page) {
    return page->physical_addr;
}

#endif /* _AMETHYST_MEM_PAGE_H */

