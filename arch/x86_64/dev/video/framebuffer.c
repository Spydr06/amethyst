#include <multiboot2.h>
#include <kernelio.h>
#include <stdint.h>

#include <x86_64/mem/paging.h>

void __framebuffer_map_page(const struct multiboot_tag_framebuffer* tag) {
    uint64_t phys_addr = (uint64_t) tag->common.framebuffer_addr;

    phys_addr |= PAGE_TABLE_ENTRY;
    *(uint64_t*) (((void*) p2_table) + 8 * 488) = phys_addr;
    phys_addr += PAGE_SIZE;
    phys_addr |= PAGE_TABLE_ENTRY;
    *(uint64_t*) (((void*) p2_table) + 8 * 489) = phys_addr;

    phys_addr = (uint64_t) tag->common.framebuffer_addr;

    uint32_t mem_size = (tag->common.framebuffer_pitch * tag->common.framebuffer_height); 

    uint32_t entries = mem_size / PAGE_SIZE;
    uint32_t entries_mod = mem_size % PAGE_SIZE;
    
    uint32_t pd = PD_ENTRY(FRAMEBUFFER_MEM_START);
    uint32_t pdpr = PDPR_ENTRY(FRAMEBUFFER_MEM_START);
    uint32_t pml4 = PML4_ENTRY(FRAMEBUFFER_MEM_START);

    if(p4_table[pml4] == 0 && p3_table_hh[pdpr] == 0)
        panic("PML4 or PDPR empty - could not initialize framebuffer.");
    
    if(entries_mod != 0)
        entries++;

    for(size_t i = 0; entries > 0; i++) {
        entries--;
        if((p2_table[pd + i] < phys_addr ||
            p2_table[pd + i] > (phys_addr + mem_size)) ||
            p2_table[pd + i] == 0) {
                p2_table[pd + i] = (phys_addr + (i * PAGE_SIZE)) | PAGE_TABLE_ENTRY;
        }
    }
}
