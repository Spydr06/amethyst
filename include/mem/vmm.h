#ifndef _AMETHYST_MEM_VMM_H
#define _AMETHYST_MEM_VMM_H

#include <mem/mmap.h>
#include <mem/pmm.h>

#include <sys/mutex.h>

#include <stddef.h>
#include <stdint.h>

#ifdef __x86_64__
#include <x86_64/mem/mmu.h>
#define VMM_RESERVED_SPACE_SIZE 0x14480000000
#endif

#define VMM_RANGES_PER_CACHE ((PAGE_SIZE - sizeof(struct vmm_cache_header)) / sizeof(struct vmm_range))

enum vmm_flags : uint8_t {
    VMM_FLAGS_PAGESIZE = 1,
    VMM_FLAGS_ALLOCATE = 2,
    VMM_FLAGS_PHYSICAL = 4,
    VMM_FLAGS_FILE     = 8,
    VMM_FLAGS_EXACT    = 16,
    VMM_FLAGS_SHARED   = 32,

    VMM_PERMANENT_FLAGS_MASK = (VMM_FLAGS_FILE | VMM_FLAGS_SHARED)
};

enum vmm_action : uint8_t {
    VMM_ACTION_READ  = 1,
    VMM_ACTION_WRITE = 2,
    VMM_ACTION_EXEC  = 4
};

struct vmm_range {
    struct vmm_range* next;
    struct vmm_range* prev;

    void* start;
    size_t size;
    enum vmm_flags flags;
    enum mmu_flags mmu_flags;
    
    struct vnode* vnode;
    size_t offset;
};

struct vmm_cache_header {
    mutex_t lock;
    struct vmm_cache* next;
    size_t free_count;
    uintmax_t first_free;
};

struct vmm_cache {
    struct vmm_cache_header header;
    struct vmm_range ranges[VMM_RANGES_PER_CACHE];
};

struct vmm_space {
    mutex_t lock;
    mutex_t pflock;
    struct vmm_range* ranges;
    void* start;
    void* end;
};

struct vmm_context {
    struct vmm_space space;
    page_table_ptr_t page_table;
};

struct vmm_file_desc {
    struct vnode* node;
    uintmax_t offset;
};

static_assert(sizeof(struct vmm_cache) <= PAGE_SIZE);

extern struct vmm_context vmm_kernel_context;

void vmm_init(struct mmap* mmap);
void vmm_apinit(void);

struct vmm_context* vmm_context_new(void);
void vmm_context_destroy(struct vmm_context* context);

void vmm_switch_context(struct vmm_context* context);

void* vmm_map(void* addr, size_t size, enum vmm_flags flags, enum mmu_flags mmu_flags, void* private);
void vmm_unmap(void* addr, size_t size, enum vmm_flags flags);

void vmm_cache_init(void);

int vmm_cache_truncate(struct vnode* vnode, uintmax_t offset);
int vmm_cache_make_dirty(struct page* page);
int vmm_cache_get_page(struct vnode* vnode, uintptr_t offset, struct page** res);

#endif /* _AMETHYST_MEM_VMM_H */

