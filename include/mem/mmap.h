#ifndef _AMETHYST_MEM_MMAP_H
#define _AMETHYST_MEM_MMAP_H

#include <stdint.h>
#include <stddef.h>
#include <cdefs.h>

#include <limine/limine.h>

struct mmap {
    struct limine_memmap_response* map;
    size_t total_memory;
};

int mmap_parse(struct mmap* mmap);
const char* mmap_strtype(uint64_t);

#endif /* _AMETHYST_MEM_MMAP_H */

