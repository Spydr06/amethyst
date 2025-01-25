#ifndef _AMETHYST_BITMAP_H
#define _AMETHYST_BITMAP_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#define BLOCK_SIZE 4096
#define BLOCKS_PER_BYTE 8
#define INVALID_BLOCK ((size_t) -1)

struct bitmap {
    uint8_t* bitmap;

    size_t block_cnt;
    size_t byte_cnt;
    size_t last_deep_fragmented;
    size_t mem_start;
};

typedef bool bit_t;

bit_t bitmap_get(struct bitmap* bitmap, size_t block);
void bitmap_set(struct bitmap* bitmap, size_t block, bit_t value);

void* bitmap_allocate(struct bitmap* bitmap, size_t blocks);

void bitmap_mark_blocks(struct bitmap* bitmap, size_t start, size_t size, bit_t value);
void bitmap_mark_region(struct bitmap* bitmap, void* base, size_t byte_cnt, bit_t value);

#endif /* _AMETHYST_BITMAP_H */

