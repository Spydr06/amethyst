#include <mem/pmm.h>
#include <mem/vmm.h>
#include <mem/mmap.h>
#include <sys/spinlock.h>

#include <assert.h>
#include <bitmap.h>
#include <hashtable.h>
#include <kernelio.h>
#include <math.h>
#include <string.h>

#include <limine/limine.h>

static struct bitmap physical;
static spinlock_t pmm_lock;

static hashtable_t refcounts;
static spinlock_t refcount_lock;

static struct mmap pmm_mmap;

void* pmm_zero_page;

static inline void refcount_table_init(void) {
    assert(hashtable_init(&refcounts, 0x2000 /* arbitrary */) == 0);
}

static inline size_t get_refcount(void* addr) {
    assert((uintptr_t) addr % PAGE_SIZE == 0);

    if(!refcounts.capacity)
        return 0;

    void* rc;
    int err = hashtable_get(&refcounts, &rc, &addr, sizeof(void*));
    if(err)
        return 0;
    return (size_t) rc;
}

static inline void set_refcount(void* addr, size_t rc) {
    if(!refcounts.capacity)
        refcount_table_init();

    assert((uintptr_t) addr % PAGE_SIZE == 0);

    assert(hashtable_set(&refcounts, (void*) rc, &addr, sizeof(void*), true) == 0);
}

uintptr_t hhdm_base;

static volatile struct limine_hhdm_request hhdm_request = {
    .id = LIMINE_HHDM_REQUEST,
    .revision = 0
};

void pmm_init(struct mmap* mmap) {
    assert(hhdm_request.response);
    hhdm_base = hhdm_request.response->offset;

    memcpy(&pmm_mmap, mmap, sizeof(struct mmap));

    klog(INFO, "total memory: 0x%zx bytes (%Zu)", mmap->total_memory, mmap->total_memory);

    physical.block_cnt = ROUND_UP_DIV(mmap->total_memory, BLOCK_SIZE);
    physical.byte_cnt = ROUND_UP_DIV(physical.block_cnt, 8);
    
    struct limine_memmap_entry* mm = 0;

    for(size_t i = 0; i < mmap->map->entry_count; i++) {
        struct limine_memmap_entry* entry = mmap->map->entries[i];
        if(entry->type != LIMINE_MEMMAP_USABLE || entry->length < physical.byte_cnt)
            continue;
        mm = entry;
        break;
    }

    if(!mm)
        panic("Out of Memory (pmm bitmap[%zu])", physical.byte_cnt);
    
    klog(INFO, "Using %zu bytes (%Zu) for pmm bitmap.", physical.byte_cnt, physical.byte_cnt);

    void* bitmap_start_phys = (void*) mm->base;
    physical.bitmap = MAKE_HHDM(bitmap_start_phys);
    memset(physical.bitmap, 0xff, physical.byte_cnt);

    for(size_t i = 0; i < mmap->map->entry_count; i++) {
        struct limine_memmap_entry* entry = mmap->map->entries[i];
        if(entry->type == LIMINE_MEMMAP_USABLE)
            bitmap_mark_region(&physical, (void*) entry->base, entry->length, 0);
    }

    for(size_t i = 0; i < mmap->map->entry_count; i++) {
        struct limine_memmap_entry* entry = mmap->map->entries[i];
        if(entry->type != LIMINE_MEMMAP_USABLE)
            bitmap_mark_region(&physical, (void*) entry->base, entry->length, 1);
    }

    bitmap_mark_region(&physical, bitmap_start_phys, physical.byte_cnt, 1);

    spinlock_init(pmm_lock);
    spinlock_init(refcount_lock);

    pmm_zero_page = pmm_alloc_page(PMM_SECTION_DEFAULT);
    memset(MAKE_HHDM(pmm_zero_page), 0, PAGE_SIZE);
}

void* pmm_alloc(size_t size, enum pmm_section_type section) {
    assert(section == PMM_SECTION_DEFAULT && "1MiB and 4GiB PMM sections are not yet implemented.");
    if(!size)
        return nullptr;

    spinlock_acquire(&pmm_lock);
    void* phys = bitmap_allocate(&physical, size);
    spinlock_release(&pmm_lock);

    if(!phys)
        panic("Out of memory!");
    
    return phys;
}

void pmm_free(void* addr, size_t count) {
    spinlock_acquire(&pmm_lock);

    size_t rc = get_refcount(addr);

    if(rc <= 1)
        bitmap_mark_region(&physical, addr, count * BLOCK_SIZE, 0);
    else
        set_refcount(addr, rc - 1);

    spinlock_release(&pmm_lock);
}

uintmax_t pmm_total_memory(void) {
    return (uintmax_t) pmm_mmap.total_memory;
}

uintmax_t pmm_free_memroy(void) {
    return 0;
}

void pmm_hold(void* addr) {
    assert((uintptr_t) addr % PAGE_SIZE == 0);

    spinlock_acquire(&refcount_lock);
 
    if(!refcounts.capacity)
        refcount_table_init();

    size_t rc = 0;
    int err = hashtable_get(&refcounts, (void**) &rc, &addr, sizeof(void*));
    if(err) {
        if(bitmap_get(&physical, (size_t) addr))
            rc += 1;
    }

    set_refcount(addr, rc + 1);

    spinlock_release(&refcount_lock);
}

void pmm_release(void* addr) {
    spinlock_acquire(&refcount_lock);

    size_t rc = get_refcount(addr);
    if(rc == 0)
        pmm_free_page(addr);
    else if(rc == 1) {
        hashtable_remove(&refcounts, &addr, sizeof(void*));
        pmm_free_page(addr);
    }
    else
        set_refcount(addr, rc - 1);
        
    spinlock_release(&refcount_lock);
}
