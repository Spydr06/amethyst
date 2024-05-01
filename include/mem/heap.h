#ifndef _AMETHYST_MEM_HEAP_H
#define _AMETHYST_MEM_HEAP_H

#include <stddef.h>
#include <string.h>

void kernel_heap_init(void);

void* kmalloc(size_t size);
void* krealloc(void* ptr, size_t size);
void kfree(void* ptr);

inline void* kcmalloc(size_t n, size_t size) {
    void* ptr = kmalloc(n * size);
    memset(ptr, 0, n * size);
    return ptr;
}

#endif /* _AMETHYST_MEM_HEAP_H */

