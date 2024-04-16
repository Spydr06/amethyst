#include "mem/bitmap.h"
#include "mem/pmm.h"
#include <mem/mmap.h>
#include <kernelio.h>

const char* mmap_types[] = {
    "Invalid",
    "Available",
    "Reserved",
    "Reclaimable",
    "NVS",
    "Defective"
};

static uint32_t mmap_number_of_entries;
static struct multiboot_mmap_entry* mmap_entries;
static uint8_t num_physical_reserved;

void mmap_parse(const struct multiboot_tag_mmap* root) {
    mmap_number_of_entries = (root->size - sizeof(struct multiboot_tag_mmap)) / root->entry_size; 
    mmap_entries = (struct multiboot_mmap_entry*) root->entries;

#ifndef NDEBUG
    uint32_t i = 0;
    while(i<mmap_number_of_entries){
        klog(DEBUG, "    (%2d) Address: %p, len: %p, type: (%d) %s", i, (void*) mmap_entries[i].addr, (void*) mmap_entries[i].len, mmap_entries[i].type, (char *) mmap_types[mmap_entries[i].type]);
        i++;
    }
    klog(DEBUG, "    Total entries: %d", i);
#endif
}

void mmap_setup(const struct multiboot_tag_basic_meminfo* entry) {
    num_physical_reserved = 0;
    if(used_frames > 0) {
        uint32_t counter = 0;
        uint64_t mem_limit = (entry->mem_upper + 1024) * 1024;
        while(counter < mmap_number_of_entries) {
            if(mmap_entries[counter].addr < mem_limit && mmap_entries[counter].type > 1) {
                klog(DEBUG, "Found usable MMAP entry at addr %p.", (void*) mmap_entries[counter].addr);
                pmm_reserve_area(mmap_entries[counter].addr, mmap_entries[counter].len);
                num_physical_reserved++;
            }
            counter++;
        }
    }
}

uintptr_t mmap_determine_bitmap_region(uint64_t lower_limit, size_t size) {
    for(size_t i = 0; i < mmap_number_of_entries; i++) {
        struct multiboot_mmap_entry* entry = &mmap_entries[i];

        if(entry->type != MMAP_AVAILABLE || entry->addr + entry->len < lower_limit)
            continue;

        size_t offset = lower_limit > entry->addr ? lower_limit - entry->addr : 0;
        size_t actual_available_space = entry->len - offset;
        if(actual_available_space >= size) {
            klog(DEBUG, "PMM bitmap is at address %p", (void*) (entry->addr + offset));
            return entry->addr + offset;
        }
    }

    panic("Could not find space big enough to hold the PMM bitmap.");
}

