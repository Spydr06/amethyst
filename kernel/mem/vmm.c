#include <mem/vmm.h>
#include <mem/pmm.h>

#include <kernelio.h>
#include <assert.h>
#include <math.h>

#define RANGE_TOP(range) ((void*) ((uintptr_t) (range)->start + (range)->size))

static struct vmm_context kernel_context;
static struct vmm_space kernel_space = {
    .start = KERNELSPACE_START,
    .end = KERNELSPACE_END
};

static struct vmm_cache* cache_list;

static struct vmm_cache* new_cache(void);
static struct vmm_space* get_space(void* vaddr);

static void free_range(struct vmm_range* range);
static struct vmm_range* alloc_range(void);
static void* get_free_range(struct vmm_space* space, void* addr, size_t offset);
static void insert_range(struct vmm_space* space, struct vmm_range* range);
static void unmap(struct vmm_space* space, void* addr, size_t size);

void vmm_init(struct mmap* mmap) {
    mutex_init(&kernel_space.lock);
    mutex_init(&kernel_space.pflock);

    cache_list = new_cache();
    kernel_context.page_table = mmu_new_table();
    assert(cache_list && kernel_context.page_table);

    kernel_context.space.start = USERSPACE_START;
    kernel_context.space.end = USERSPACE_END;

    vmm_switch_context(&kernel_context);

    // map hhdm
    for(uint64_t i = 0; i < mmap->mmap->entry_count; i++) {
        const struct limine_memmap_entry* entry = mmap->mmap->entries[i];
        assert(vmm_map(MAKE_HHDM(entry->base), entry->length, VMM_FLAGS_EXACT, MMU_FLAGS_READ | MMU_FLAGS_WRITE | MMU_FLAGS_NOEXEC, nullptr));
    }
}

void vmm_apinit(void) {
    vmm_switch_context(&kernel_context);
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

    mutex_acquire(&space->pflock, false);
    mutex_acquire(&space->lock, false);
    unmap(space, addr, size);
    mutex_release(&space->lock);
    mutex_release(&space->pflock);
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

static void unmap(struct vmm_space* space, void* addr, size_t size) {
    (void) space;
    (void) addr;
    (void) size;
    unimplemented();
}

