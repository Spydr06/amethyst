#include <mem/mmap.h>
#include <kernelio.h>
#include <mem/pmm.h>
#include <limine/limine.h>

#include <errno.h>
#include <string.h>

const char* mmap_typestrs[] = {
    "Usable",
    "Reserved",
    "ACPI Reclaimable",
    "ACPI NVS",
    "Bad Memory",
    "Bootloader Reclaimable",
    "Kernel & Modules",
    "Framebuffer"
};

static volatile struct limine_memmap_request mmap_request = {
    .id = LIMINE_MEMMAP_REQUEST,
    .revision = 0
};

int mmap_parse(struct mmap* mmap) {
    if(!mmap)
        return EINVAL;

    if(!mmap_request.response)
        return ENOMEM;

    memset(mmap, 0, sizeof(struct mmap));
    mmap->map = mmap_request.response;
    
    klog(INFO, "memory map entries:");

    for(size_t i = 0; i < mmap_request.response->entry_count; i++) {
        struct limine_memmap_entry* entry = mmap_request.response->entries[i];

        klog(INFO, " %2zu) %p -> %p: %s", i, (void*) entry->base, (void*) entry->base + entry->length, mmap_strtype(entry->type));

        if(entry->type != LIMINE_MEMMAP_RESERVED)
            mmap->total_memory += entry->length;
    }

    return 0;
}

const char* mmap_strtype(uint64_t type) {
    return mmap_typestrs[type];
}

