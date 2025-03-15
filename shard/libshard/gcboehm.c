#define _LIBSHARD_INTERNAL
#include <libshard.h>

#ifdef SHARD_USE_GCBOEHM

#include <gc.h>

#define GCBOEHM_MAGIC_NUMBER ((void*) 0x12345678)

const enum shard_gc_backend _shard_gc_backend = SHARD_GC_BOEHM;

struct shard_gc* shard_gc_begin(struct shard_context* ctx, void* stack_base) {
    (void) ctx;
    (void) stack_base;
    GC_INIT();
    return (void*) GCBOEHM_MAGIC_NUMBER;
}

void shard_gc_end(struct shard_gc* gc) {
    assert(gc == GCBOEHM_MAGIC_NUMBER);
}

void shard_gc_pause(volatile struct shard_gc* gc) {
    assert(gc == GCBOEHM_MAGIC_NUMBER);
    GC_disable();
}

void shard_gc_resume(volatile struct shard_gc* gc) {
    assert(gc == GCBOEHM_MAGIC_NUMBER);
    GC_enable();
}

void shard_gc_run(volatile struct shard_gc* gc) {
    assert(gc == GCBOEHM_MAGIC_NUMBER);
    GC_gcollect();
}

void* shard_gc_make_static(volatile struct shard_gc* gc, void* ptr) {
    assert(gc == GCBOEHM_MAGIC_NUMBER);
    size_t size = GC_size(ptr);
    GC_add_roots(ptr, ptr + size);
    return ptr;
}

void* shard_gc_malloc(volatile struct shard_gc* gc, size_t size) {
    assert(gc == GCBOEHM_MAGIC_NUMBER);
    return GC_MALLOC(size);
}

void* shard_gc_calloc(volatile struct shard_gc* gc, size_t nmemb, size_t size) {
    assert(gc == GCBOEHM_MAGIC_NUMBER);
    void* ptr = GC_MALLOC(nmemb * size);
    memset(ptr, 0, nmemb * size);
    return ptr;
}

void* shard_gc_realloc(volatile struct shard_gc* gc, void* ptr, size_t size) {
    assert(gc == GCBOEHM_MAGIC_NUMBER);
    return GC_REALLOC(ptr, size);
}

void shard_gc_free(volatile struct shard_gc* gc, void* ptr) {
    assert(gc == GCBOEHM_MAGIC_NUMBER);
    GC_FREE(ptr);
}

char* shard_gc_strdup(volatile struct shard_gc* gc, const char* str, size_t size) {
    assert(gc == GCBOEHM_MAGIC_NUMBER);
    return GC_STRNDUP(str, size);
}

#endif /* SHARD_USE_GCBOEHM */

