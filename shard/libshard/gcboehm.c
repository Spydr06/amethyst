#define _LIBSHARD_INTERNAL
#include <libshard.h>

#ifdef SHARD_USE_GCBOEHM

#include <gc.h>

const enum shard_gc_backend _shard_gc_backend = SHARD_GC_BOEHM;

void shard_gc_begin(volatile struct shard_gc* gc, struct shard_context* ctx, void* stack_base) {
    (void) gc;
    (void) ctx;
    (void) stack_base;
    GC_INIT();
}

void shard_gc_end(volatile struct shard_gc* gc) {
    (void) gc;
}

void* shard_gc_malloc(volatile struct shard_gc* gc, size_t size) {
    (void) gc;
    return GC_MALLOC(size);
}

void* shard_gc_calloc(volatile struct shard_gc* gc, size_t nmemb, size_t size) {
    (void) gc;
    void* ptr = GC_MALLOC(nmemb * size);
    memset(ptr, 0, nmemb * size);
    return ptr;
}

void* shard_gc_realloc(volatile struct shard_gc* gc, void* ptr, size_t size) {
    (void) gc;
    return GC_REALLOC(ptr, size);
}

void shard_gc_free(volatile struct shard_gc* gc, void* ptr) {
    (void) gc;
    GC_FREE(ptr);
}

#endif /* SHARD_USE_GCBOEHM */

