#include <mem/heap.h>

#include <mem/slab.h>
#include <mem/mmap.h>
#include <mem/vmm.h>

#include <assert.h>
#include <kernelio.h>
#include <string.h>

#define ALIGN(size) (((size) / KERNEL_HEAP_ALLOC_ALIGN + 1) * KERNEL_HEAP_ALLOC_ALIGN)

#define CACHE_COUNT 12

static size_t allocsizes[CACHE_COUNT] = {32, 64, 128, 256, 512, 1024, 2048, 4096, 8192, 16384, 32768, 65536};
static struct scache* caches[CACHE_COUNT];

static void init_area(struct scache* cache, void* obj) {
    size_t* ptr = obj;
    *ptr = cache->size - sizeof(size_t) * 2;
    memset(ptr + 2, 0, *ptr);
}

static void dtor(struct scache* cache, void* obj) {
    init_area(cache, obj);
}

static struct scache* get_cache_from_size(size_t size) {
    struct scache* cache = NULL;
    for (int i = 0; i < CACHE_COUNT; ++i) {
		if (size <= allocsizes[i]) {
			cache = caches[i];
			break;
		}
	}
    assert(cache);
    return cache;
}

void kernel_heap_init(void) {
    for(int i = 0; i < CACHE_COUNT; i++) {
        caches[i] = slab_newcache(allocsizes[i] + sizeof(size_t) * 2, 0, init_area, dtor);
        assert(caches[i]);
    }
}

void* kmalloc(size_t size) {
    struct scache* cache = get_cache_from_size(size);
    size_t* ret = slab_alloc(cache);
    if(!ret)
        return nullptr;

    ret[0] = cache->size - sizeof(size_t) * 2;
//    assert(ret[0] == (cache->size - sizeof(size_t) * 2));
    ret[1] = size;
    return ret + 2;
}

void* krealloc(void* ptr, size_t size) {
    size_t *start = ((size_t*) ptr) - 2;
    size_t current_size = start[1];

    if(size < current_size) {
        start[1] = size;
        return ptr;
    }

    struct scache* old_cache = get_cache_from_size(start[0]);
    struct scache* new_cache = get_cache_from_size(size);

    // reuse same allocation
    if(old_cache == new_cache) {
        size_t diff = size - current_size;
        memset((void*)((uintptr_t) ptr + current_size), 0, diff);
        start[1] = size;
        return ptr;
    }

    // new allocation
    size_t* new = slab_alloc(new_cache);
    if(!new)
        return nullptr;

    new[1] = size;
    memcpy(new + 2, ptr, current_size);
    slab_free(old_cache, start);
    return new + 2;
}

void kfree(void* ptr) {
    size_t* start = ((size_t*) ptr) - 2;
    size_t size = start[0];
    struct scache* cache = get_cache_from_size(size);
    slab_free(cache, start);
}

