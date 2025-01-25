#include <mem/pmm.h>
#include <mem/vmm.h>
#include <mem/mmap.h>
#include <sys/spinlock.h>

#include <math.h>
#include <bitmap.h>
#include <kernelio.h>
#include <string.h>
#include <assert.h>

#include <limine/limine.h>

static struct bitmap physical;
static spinlock_t pmm_lock;

uintptr_t hhdm_base;

static volatile struct limine_hhdm_request hhdm_request = {
    .id = LIMINE_HHDM_REQUEST,
    .revision = 0
};

void pmm_init(struct mmap* mmap) {
    assert(hhdm_request.response);
    hhdm_base = hhdm_request.response->offset;

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
    
    klog(INFO, "Using %zu bytes for pmm bitmap.", physical.byte_cnt);

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
}

void* pmm_alloc(size_t size, enum pmm_section_type section) {
    (void) section; // TODO: reimplement differently sized sections (1MiB, 4GiB)
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
    bitmap_mark_region(&physical, addr, count * BLOCK_SIZE, 0);
    spinlock_release(&pmm_lock);
}


