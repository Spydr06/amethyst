#ifndef _AMETHYST_MEM_PMM_H
#define _AMETHYST_MEM_PMM_H

#include <stddef.h>
#include <stdint.h>

extern size_t memory_size_in_bytes;

void pmm_setup(uintptr_t addr, uint32_t size);
void* pmm_alloc_frame(void);
void pmm_reserve_area(uintptr_t starting_addr, size_t size);

#endif /* _AMETHYST_MEM_PMM_H */

