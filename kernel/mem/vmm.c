#include "x86_64/mem/mmu.h"
#include <mem/vmm.h>
#include <mem/pmm.h>
#include <mem/slab.h>

#include <filesystem/virtual.h>
#include <sys/proc.h>
#include <sys/thread.h>

#include <kernelio.h>
#include <assert.h>
#include <errno.h>
#include <math.h>
#include <cdefs.h>

#define RANGE_TOP(range) ((void*) ((uintptr_t) (range)->start + (range)->size))

struct vmm_context vmm_kernel_context;
static struct vmm_space kernel_space = {
    .start = KERNELSPACE_START,
    .end = KERNELSPACE_END
};

static struct vmm_cache* cache_list;
static struct scache* ctx_cache;

static struct vmm_cache* new_cache(void);
static struct vmm_space* get_space(void* vaddr);

static void free_range(struct vmm_range* range);
static struct vmm_range* alloc_range(void);
static void* get_free_range(struct vmm_space* space, void* addr, size_t offset);
static void insert_range(struct vmm_space* space, struct vmm_range* range);
static int change_map(struct vmm_space* space, void* addr, size_t size, bool free, enum vmm_flags flags, enum mmu_flags new_mmu_flags);

void vmm_init(struct mmap* mmap) {
    mutex_init(&kernel_space.lock);
    mutex_init(&kernel_space.pflock);

    cache_list = new_cache();
    vmm_kernel_context.page_table = mmu_new_table();
    assert(cache_list && vmm_kernel_context.page_table);

    vmm_kernel_context.space.start = USERSPACE_START;
    vmm_kernel_context.space.end = USERSPACE_END;

    vmm_switch_context(&vmm_kernel_context);

    // map hhdm
    for(uint64_t i = 0; i < mmap->mmap->entry_count; i++) {
        const struct limine_memmap_entry* entry = mmap->mmap->entries[i];
        assert(vmm_map(MAKE_HHDM(entry->base), entry->length, VMM_FLAGS_EXACT, MMU_FLAGS_READ | MMU_FLAGS_WRITE | MMU_FLAGS_NOEXEC, nullptr));
    }
}

void vmm_apinit(void) {
    vmm_switch_context(&vmm_kernel_context);
}

static void ctx_ctor(struct scache* cache __unused, void* obj) {
    struct vmm_context* ctx = obj;

    ctx->space.start = USERSPACE_START;
    ctx->space.end = USERSPACE_END;

    mutex_init(&ctx->space.lock);
    mutex_init(&ctx->space.pflock);
    ctx->space.ranges = nullptr;
}

// dtor as alias to ctor
static void (*ctx_dtor)(struct scache*, void*) = &ctx_ctor;

struct vmm_context* vmm_context_new(void) { 
    if(!ctx_cache)
        assert(ctx_cache = slab_newcache(sizeof(struct vmm_context), 0, ctx_ctor, ctx_dtor));

    struct vmm_context* ctx = slab_alloc(ctx_cache);
    if(!ctx)
        return nullptr;

    if(!(ctx->page_table = mmu_new_table())) {
        slab_free(ctx_cache, ctx);
        return nullptr;
    }
    
    return ctx;
}

void vmm_context_destroy(struct vmm_context* context) {
    if(!context)
        return;

    struct vmm_context* old_ctx = _cpu()->thread->vmm_context;
    vmm_switch_context(context);
    vmm_unmap(context->space.start, context->space.end - context->space.start, 0);
    vmm_switch_context(old_ctx);
    mmu_destroy_table(context->page_table);
    slab_free(ctx_cache, context);
}

void vmm_switch_context(struct vmm_context* context) {
    if(_cpu()->thread)
        _cpu()->thread->vmm_context = context;
    _cpu()->vmm_context = context;
    mmu_switch(context->page_table);
}

void* vmm_map(void* addr, size_t size, enum vmm_flags flags, enum mmu_flags mmu_flags, void* private) {
    if(!addr)
        addr = KERNELSPACE_START;

    addr = (void*) ROUND_DOWN((uintptr_t) addr, PAGE_SIZE);
    if(flags & VMM_FLAGS_PAGESIZE)
        size *= PAGE_SIZE;
    else
        size = ROUND_UP(size, PAGE_SIZE);

    if(!size)
        return nullptr;

    struct vmm_space* space = get_space(addr);
    if(!space)
        return NULL;

    mutex_acquire(&space->lock, false);
    struct vmm_range* range = nullptr;

    void* start = get_free_range(space, addr, size);
    void* ret_addr = nullptr;

    if(((flags & VMM_FLAGS_EXACT) && start != addr) || !start)
        goto cleanup;

    if(!(range = alloc_range()))
        goto cleanup;

    range->start = start;
    range->size = size;
    range->flags = VMM_PERMANENT_FLAGS_MASK & flags;
    range->mmu_flags = mmu_flags;
    
    if(flags & VMM_FLAGS_FILE) {
        unimplemented();
    }

    if(flags & VMM_FLAGS_PHYSICAL) {
        for(uintmax_t i = 0; i < size; i += PAGE_SIZE) {
            if(mmu_map(_cpu()->vmm_context->page_table, (void*)((uintptr_t) private + i), (void*)((uintptr_t) start + i), mmu_flags))
                continue;
                
            for(uintmax_t j = 0; j < size; j += PAGE_SIZE)
                mmu_unmap(_cpu()->vmm_context->page_table, (void*)((uintptr_t) start + j)) ;

            goto cleanup;
        }
    }
    else if(flags & VMM_FLAGS_ALLOCATE) {
        for(uintmax_t i = 0; i < size; i += PAGE_SIZE) {
            void* allocated = pmm_alloc_page(PMM_SECTION_DEFAULT);
            if(!allocated) {
                ret_addr = nullptr;
                goto cleanup;
            }

            if(!mmu_map(_cpu()->vmm_context->page_table, allocated, (void*)((uintptr_t) start + i), mmu_flags)) {
                for(uintmax_t j = 0; j < size; j += PAGE_SIZE) {
                    void* virt = (void*)((uintptr_t) start + i);
                    void* phys = mmu_get_physical(_cpu()->vmm_context->page_table, virt);

                    if(phys) {
                        pmm_release(phys);
                        mmu_unmap(_cpu()->vmm_context->page_table, virt);
                    }
                }
            }
        }
    }

    insert_range(space, range);

    ret_addr = start;

cleanup:
    if(!start && range)
        free_range(range);

    mutex_release(&space->lock);
    return ret_addr;
}

void vmm_unmap(void* addr, size_t size, enum vmm_flags flags) {
    addr = (void*) ROUND_DOWN((uintptr_t) addr, PAGE_SIZE);

    if(flags & VMM_FLAGS_PAGESIZE)
        size *= PAGE_SIZE;
    else
        size = ROUND_UP(size, PAGE_SIZE);

    if(!size)
        return;

    struct vmm_space* space = get_space(addr);
    if(!space)
        return;

    mutex_acquire(&space->lock, false);
    change_map(space, addr, size, true, flags, 0);
    mutex_release(&space->lock);
}

static struct vmm_cache* new_cache(void) {
    struct vmm_cache* ptr = pmm_alloc_page(PMM_SECTION_DEFAULT);
    if(!ptr)
        return nullptr;

    ptr = MAKE_HHDM(ptr);
    ptr->header.free_count = VMM_RANGES_PER_CACHE;
    ptr->header.first_free = 0;
    ptr->header.next = nullptr;

    mutex_init(&ptr->header.lock);

    for(uintmax_t i = 0; i < VMM_RANGES_PER_CACHE; i++) {
        ptr->ranges[i].size = 0;
        ptr->ranges[i].next = nullptr;
    }

    return ptr;
}

static struct vmm_space* get_space(void* vaddr) {
    if(USERSPACE_START <= vaddr && vaddr < USERSPACE_END)
        return &_cpu()->vmm_context->space;
    else if(KERNELSPACE_START <= vaddr && vaddr < KERNELSPACE_END)
        return &kernel_space;
    else
        return nullptr;
}

static void* get_free_range(struct vmm_space* space, void* addr, size_t size) {
    struct vmm_range* range = space->ranges;
    if(!addr)
        addr = space->start;

    if(!range)
        return addr;

    if(range->start != space->start && addr < range->start && (uintptr_t) range->start - (uintptr_t) addr >= size)
        return addr;

    while(range->next) {
        void* range_top = RANGE_TOP(range);
        if(addr < range_top)
            addr = range_top;

        if(addr < range->next->start) {
            size_t free_size = (uintptr_t) range->next->start - (uintptr_t) addr;
            if(free_size >= size)
                return addr;
        }

        range = range->next;
    }

    void* range_top = RANGE_TOP(range);
    if(addr < range_top)
        addr = range_top;

    if(addr != space->end && (uintptr_t) space->end - (uintptr_t) addr >= size)
        return addr;

    return nullptr;
}

static uintmax_t get_entry_number(struct vmm_cache* cache) {
    for(uintmax_t i = cache->header.first_free; i < VMM_RANGES_PER_CACHE; i++) {
        if(cache->ranges[i].size == 0)
            return i;
    }
     
    // get_entry_number() called on full cache
    unreachable();
}

static void destroy_range(struct vmm_range* range, uintmax_t offset, size_t size, int flags) {
    (void) range;
    (void) offset;
    (void) size;
    (void) flags;
    unimplemented();
}

static void free_range(struct vmm_range* range) {
    struct vmm_cache* cache = (struct vmm_cache*) ROUND_DOWN((uintptr_t) range, PAGE_SIZE);
    mutex_acquire(&cache->header.lock, false);

    uintptr_t range_offset = ((uintptr_t) range - (uintptr_t) cache->ranges) / sizeof(struct vmm_range);
    range->size = 0;
    cache->header.free_count++;
    if(cache->header.first_free > range_offset)
        cache->header.first_free = range_offset;

    mutex_release(&cache->header.lock);
}

static struct vmm_range* alloc_range(void) {
    struct vmm_cache* cache = cache_list;
    struct vmm_range* range = nullptr;

    while(cache) {
        mutex_acquire(&cache->header.lock, false);
        if(cache->header.free_count > 0) {
            cache->header.free_count--;
            uintmax_t r = get_entry_number(cache);
            cache->header.first_free = r + 1;
            range = &cache->ranges[r];
            range->size = -1;
            mutex_release(&cache->header.lock);
            break;
        }
        else if(!cache->header.next)
            cache->header.next = new_cache();

        struct vmm_cache* next = cache->header.next;
        mutex_release(&cache->header.lock);
        cache = next;
    }

    return range;
}

static void insert_range(struct vmm_space* space, struct vmm_range* new_range) {
    struct vmm_range* range = space->ranges;

    if(!range) {
        space->ranges = new_range;
        new_range->next = nullptr;
        new_range->prev = nullptr;
        return;
    }

    void* new_range_top = RANGE_TOP(new_range);

    if(new_range_top <= range->start) {
        space->ranges = new_range;
        new_range->next = range;
        range->prev = new_range;
        new_range->prev = nullptr;
        goto fragmentation_check;
    }

    while(range->next) {
        if(new_range->start >= RANGE_TOP(range) && new_range->start < range->next->start) {
            new_range->next = range->next;
            new_range->prev = range;
            range->next = new_range;
            goto fragmentation_check;
        }
        range = range->next;
    }

    range->next = new_range;
    new_range->prev = range;
    new_range->next = nullptr;

fragmentation_check:
    
    if(new_range->next && new_range->next->start == new_range_top && new_range->flags == new_range->next->flags && new_range->mmu_flags == new_range->next->mmu_flags) {
        struct vmm_range* old_range = new_range->next;
        new_range->size += old_range->size;
        new_range->next = old_range->next;
        if(old_range->next)
            old_range->next->prev = new_range;

        free_range(old_range);
    }

    if(new_range->prev && RANGE_TOP(new_range->prev) == new_range->start && new_range->flags == new_range->prev->flags && new_range->mmu_flags == new_range->prev->mmu_flags) {
        struct vmm_range* old_range = new_range->prev;
        old_range->size += new_range->size;
        old_range->next = new_range->next;

        if(new_range->next)
            new_range->next->prev = old_range;

        free_range(new_range);
    }
}

static void change_mmu_range(struct vmm_range* range, void* base, size_t size, enum mmu_flags new_flags) {
    (void) range;
    (void) base;
    (void) size;
    (void) new_flags;
    unimplemented();
}

static __always_inline bool vnode_writable(struct vmm_range* range) {
    vop_lock(range->vnode);
    int err = vop_access(range->vnode, V_ACCESS_WRITE, &_cpu()->thread->proc->cred);
    vop_unlock(range->vnode);
    return err == 0;
}

static int change_map(struct vmm_space* space, void* addr, size_t size, bool free, enum vmm_flags __unused flags, enum mmu_flags new_mmu_flags) {
    void* top = addr + size;
    struct vmm_range* range = space->ranges;
    struct vmm_range* new_range = nullptr;

    if(!free) {
        new_range = alloc_range();
        if(!new_range)
            return ENOMEM;
    }

    int err = 0;
    bool check_wr = !free && (range->flags & VMM_FLAGS_CREDCHECK) && (range->flags & VMM_FLAGS_SHARED) && (range->flags & VMM_FLAGS_FILE);
    
    while(range) {
        assert(range != space->ranges || !range->prev);
        void* range_top = RANGE_TOP(range);

        // change entire range
        if(range->start >= addr && range_top <= top) {
            if(free) {
                if(range->prev)
                    range->prev->next = range->next;
                else
                    space->ranges = range->next;

                if(range->next)
                    range->next->prev = range->prev;

                destroy_range(range, 0, range->size, 0);
                free_range(range);
            }
            else {
                if(check_wr && !vnode_writable(range)) {
                    err = EACCES;
                    goto finish;
                }

                change_mmu_range(range, range->start, range->size, new_mmu_flags);
                range->mmu_flags = new_mmu_flags;
            }
        }
        // split range where entire change is within the range
        else if(addr > range->start && top < range_top) {
            if(check_wr && !vnode_writable(range)) {
                err = EACCES;
                goto finish;
            }

            struct vmm_range* new = alloc_range();
            if(!range) {
                err = ENOMEM;
                goto finish;
            }

            *new = *range;
            if(free)
                destroy_range(range, (uintptr_t) addr - (uintptr_t) range->start, size, 0);

            new->start = top;
            new->size = (uintptr_t) range_top - (uintptr_t) new->start;
            range->size = (uintptr_t) addr - (uintptr_t) range->start;

            if(range->next)
                range->next->prev = new;

            new->prev = range;
            range->next = new;

            if(range->flags & VMM_FLAGS_FILE) {
                vop_hold(range->vnode);
                new->offset += range->size + size;
            }

            if(!free) {
                new_range->start = addr;
                new_range->size = size;
                new_range->flags = range->flags;
                new_range->mmu_flags = new_mmu_flags;

                change_mmu_range(range, new_range->start, new_range->size, new_range->mmu_flags);

                if(range->flags & VMM_FLAGS_FILE) {
                    new_range->vnode = range->vnode;
                    new_range->offset = range->offset;
                    vop_hold(range->vnode);
                }

                insert_range(space, new_range);
                new_range = alloc_range();
                if(!new_range) {
                    err = ENOMEM;
                    goto finish;
                }
            }
            // partially change from start
            else if(top > range->start && range->start >= addr) {
                if(check_wr && !vnode_writable(range)) {
                    err = EACCES;
                    goto finish;
                }

                size_t diff = (uintptr_t) top - (uintptr_t) range->start;
                if(free)
                    destroy_range(range, 0, diff, 0);

                range->start = (void*)((uintptr_t) range->start + diff);
                range->size -= diff;

                if(range->flags & VMM_FLAGS_FILE)
                    range->offset += diff;

                if(!free) {
                    new_range->start = (void*)((uintptr_t) range->start - diff);
                    new_range->size = diff;
                    new_range->flags = range->flags;
                    new_range->mmu_flags = new_mmu_flags;

                    change_mmu_range(range, new_range->start, new_range->size, new_range->mmu_flags);

                    if(range->flags & VMM_FLAGS_FILE) {
                        new_range->vnode = range->vnode;
                        new_range->offset = range->offset - diff;
                        vop_hold(range->vnode);
                    }

                    insert_range(space, new_range);
                    new_range = alloc_range();
                    if(!new_range) {
                        err = ENOMEM;
                        goto finish;
                    }
                }
            }
            // partially change from end
            else if(addr < range_top && range_top <= top) {
                if(check_wr && !vnode_writable(range)) {
                    err = EACCES;
                    goto finish;
                }

                size_t diff = (uintptr_t) range_top - (uintptr_t) addr;
                range->size -= diff;
                if(free)
                    destroy_range(range, range->size, diff, 0);
                else {
                    new_range->start = (void*)((uintptr_t) range->start + diff);
                    new_range->size = diff;
                    new_range->flags = range->flags;
                    new_range->mmu_flags = new_mmu_flags;

                    change_mmu_range(range, new_range->start, new_range->size, new_range->mmu_flags);

                    if(range->flags & VMM_FLAGS_FILE) {
                        new_range->vnode = range->vnode;
                        new_range->offset = range->offset + diff;
                        vop_hold(range->vnode);
                    }

                    insert_range(space, new_range);
                    new_range = alloc_range();
                    if(!new_range) {
                        err = ENOMEM;
                        goto finish;
                    }
                }
            }

            range = range->next;
        }
    }

finish:
    if(!free)
        free_range(new_range);

    return err;
} 

