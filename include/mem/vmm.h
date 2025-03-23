#ifndef _AMETHYST_MEM_VMM_H
#define _AMETHYST_MEM_VMM_H

#include <filesystem/virtual.h>
#include <mem/slab.h>
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
    VMM_FLAGS_PAGESIZE  = 1,
    VMM_FLAGS_ALLOCATE  = 2,
    VMM_FLAGS_PHYSICAL  = 4,
    VMM_FLAGS_FILE      = 8,
    VMM_FLAGS_EXACT     = 16,
    VMM_FLAGS_SHARED    = 32,
    VMM_FLAGS_CREDCHECK = 64,

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

struct brk {
    void* base;
    void* top;
    size_t limit;
};

struct vmm_context {
    struct brk brk;
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
struct vmm_context* vmm_context_fork(struct vmm_context* old_context);

void vmm_switch_context(struct vmm_context* context);

void* vmm_map(void* addr, size_t size, enum vmm_flags flags, enum mmu_flags mmu_flags, void* private);
void vmm_unmap(void* addr, size_t size, enum vmm_flags flags);

bool vmm_pagefault(void* addr, bool user, enum vmm_action actions);

void vmm_cache_init(void);

struct page* vmm_alloc_page_meta(void* physical_addr);

int vmm_cache_truncate(struct vnode* vnode, uintmax_t offset);
int vmm_cache_make_dirty(struct page* page);
int vmm_cache_get_page(struct vnode* vnode, uintptr_t offset, struct page** res);

static inline enum mmu_flags vnode_to_mmu_flags(enum vfflags flags) {
    enum mmu_flags mmu_flags = MMU_FLAGS_USER;
    if(flags & V_FFLAGS_READ)
        mmu_flags |= MMU_FLAGS_READ;
    if(flags & V_FFLAGS_WRITE)
        mmu_flags |= MMU_FLAGS_WRITE;
    if((flags & V_FFLAGS_EXEC) == 0)
        mmu_flags |= MMU_FLAGS_NOEXEC;

    return mmu_flags;
}

static inline enum vfflags mmu_to_vnode_flags(enum mmu_flags flags) {
    enum vfflags vfflags = 0;
    if(flags & MMU_FLAGS_READ)
        vfflags |= V_FFLAGS_READ;
    if(flags & MMU_FLAGS_WRITE)
        vfflags |= V_FFLAGS_WRITE;
    if(!(flags & MMU_FLAGS_NOEXEC))
        vfflags |= V_FFLAGS_EXEC;

    return vfflags;
}

static inline enum vmm_action violation_to_vmm_action(enum mmu_violation violation) {
    enum vmm_action action = 0;
    if(violation & PRESENT_VIOLATION)
        action |= VMM_ACTION_READ;
    if(violation & WRITE_VIOLATION)
        action |= VMM_ACTION_WRITE;
    if(violation & FETCH_VIOLATION)
        action |= VMM_ACTION_EXEC;
    return action;
}

static inline void vmm_action_as_str(enum vmm_action action, char str[4]) {
    str[0] = action & VMM_ACTION_READ  ? 'r' : '-';
    str[1] = action & VMM_ACTION_WRITE ? 'w' : '-';
    str[2] = action & VMM_ACTION_EXEC  ? 'x' : '-';
    str[3] = '\0';
}

#endif /* _AMETHYST_MEM_VMM_H */

