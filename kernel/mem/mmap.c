#include <mem/mmap.h>
#include <kernelio.h>
#include <mem/pmm.h>
#include <limine/limine.h>

#include <errno.h>
#include <string.h>

const char* mmap_typestrs[] = {
    "Available",
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
    mmap->mmap = mmap_request.response;
    
    klog(INFO, "memory map entries:");

    for(size_t i = 0; i < mmap_request.response->entry_count; i++) {
        struct limine_memmap_entry* entry = mmap_request.response->entries[i];

        klog(INFO, " %2zu) %p -> %p: %s", i, (void*) entry->base, (void*) entry->base + entry->length, mmap_strtype(entry->type));

        if(entry->type == MMAP_AVAILABLE || entry->type == MMAP_BOOTLOADER_RECLAIMABLE) {
            size_t section_top = entry->base + entry->length;
            if(section_top > mmap->top)
                mmap->top = section_top;
        }

        if(entry->type == MMAP_AVAILABLE) {
            if(!mmap->biggest_entry || mmap->biggest_entry->length < entry->length)
                mmap->biggest_entry = entry;
            mmap->memory_size += entry->length;
        }
    }

    return 0;
}

const char* mmap_strtype(enum mmap_type type) {
    return mmap_typestrs[type];
}

