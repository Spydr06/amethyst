#ifndef _AMETHYST_MEM_HEAP_H
#define _AMETHYST_MEM_HEAP_H

#include <stddef.h>
#include <stdint.h>

#define KERNEL_HEAP_INITIAL_SIZE   0x1000
#define KERNEL_HEAP_ALLOC_ALIGN    0x10
#define KERNEL_HEAP_MIN_ALLOC_SIZE 0x20

enum kheap_merge : uint8_t {
    KHEAP_MERGE_RIGHT = 0b01,
    KHEAP_MERGE_LEFT = 0b10
};

struct kheap_node {
    uint64_t size;
    bool is_free;
    struct kheap_node* next;
    struct kheap_node* prev;
};

void kernel_heap_init(void);

void* kmalloc(size_t size);
void* krealloc(void* ptr, size_t size);
void kfree(void* ptr);

inline void* kcmalloc(size_t n, size_t size) {
    return kmalloc(n * size);
}

#endif /* _AMETHYST_MEM_HEAP_H */

