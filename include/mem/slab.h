#ifndef _AMETHYST_MEM_SLAB_H
#define _AMETHYST_MEM_SLAB_H

#include <stddef.h>

#include <sys/spinlock.h>

struct slab {
    struct slab* next;
    struct slab* prev;

    size_t used;
    void** free;
    void* base;
};

struct scache {
    spinlock_t lock;
    
    void (*ctor)(struct scache* cache, void* obj);
    void (*dtor)(struct scache* cache, void* obj);

    struct slab* full;
    struct slab* partial;
    struct slab* empty;

    size_t size;
    size_t true_size;
    size_t align;
    size_t slab_obj_count;
};

void* slab_alloc(struct scache* cache);
void slab_free(struct scache* scache, void* addr);

struct scache* slab_newcache(size_t size, size_t align, void (*ctor)(struct scache*, void*), void (*dtor)(struct scache*, void*));
void slab_freecache(struct scache* cache);

#endif /* _AMETHYST_MEM_SLAB_H */

