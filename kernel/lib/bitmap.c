#include <bitmap.h>

#include <math.h>

static inline void* to_ptr(struct bitmap* bitmap, size_t block) {
    return (void*)(bitmap->mem_start + (block * BLOCK_SIZE));
}

static inline size_t to_block(struct bitmap* bitmap, void* ptr) {
    uint8_t* bytes = ptr;
    return (size_t) (bytes - bitmap->mem_start) / BLOCK_SIZE;
}

static inline size_t to_block_rounded(struct bitmap* bitmap, void* ptr) {
    uint8_t* bytes = ptr;
    return  (size_t) ROUND_UP_DIV((size_t)(bytes - bitmap->mem_start), BLOCK_SIZE);
}

static size_t find_free_region(struct bitmap* bitmap, size_t blocks) {
    size_t current_region_start = bitmap->last_deep_fragmented;
    size_t current_region_size = 0;

    for(size_t i = current_region_start; i < bitmap->block_cnt; i++) {
        if(bitmap_get(bitmap, i)) {
            current_region_size = 0;
            current_region_start = i + 1;
        }
        else {
            if(blocks == 1)
                bitmap->last_deep_fragmented = current_region_start + 1;

            current_region_size++;
            if(current_region_size >= blocks)
                return current_region_start;
        }
    }

    return INVALID_BLOCK;
}

bit_t bitmap_get(struct bitmap* bitmap, size_t block) {
    size_t addr = block / BLOCKS_PER_BYTE;
    size_t offset = block % BLOCKS_PER_BYTE;
    return (bitmap->bitmap[addr] & (1 << offset)) != 0;
}

void bitmap_set(struct bitmap* bitmap, size_t block, bit_t value) {
    size_t addr = block / BLOCKS_PER_BYTE;
    size_t offset = block % BLOCKS_PER_BYTE;
    if(value)
        bitmap->bitmap[addr] |= (1 << offset);
    else
        bitmap->bitmap[addr] &= ~(1 << offset);
}

void* bitmap_allocate(struct bitmap* bitmap, size_t blocks) {
    if(blocks == 0)
        return nullptr;

    size_t region = find_free_region(bitmap, blocks);
    if(region == INVALID_BLOCK)
        return 0;

    bitmap_mark_blocks(bitmap, region, blocks, 1);
    return to_ptr(bitmap, region);
}

void bitmap_mark_blocks(struct bitmap* bitmap, size_t start, size_t size, bit_t value) {
    if(!value && start < bitmap->last_deep_fragmented)
        bitmap->last_deep_fragmented = start;

    for(size_t i = start; i < start + size; i++)
        bitmap_set(bitmap, i, value);
}

void bitmap_mark_region(struct bitmap* bitmap, void* base_ptr, size_t byte_cnt, bit_t value) {
    size_t base = value ? to_block(bitmap, base_ptr) : to_block_rounded(bitmap, base_ptr);
    size_t size = value ? ROUND_UP_DIV(byte_cnt, BLOCK_SIZE) : byte_cnt / BLOCK_SIZE;

    bitmap_mark_blocks(bitmap, base, size, value);
}

