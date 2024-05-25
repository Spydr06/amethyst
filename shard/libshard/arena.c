#define _LIBSHARD_INTERNAL
#include <libshard.h>

#ifndef PAGE_SIZE
    #define PAGE_SIZE 4096
#endif

static struct shard_arena* _arena_init(const struct shard_context* ctx, size_t size) {
    struct shard_arena* arena = ctx->malloc(sizeof(struct shard_arena));
    if(!arena)
        return NULL;

    arena->region = ctx->malloc(size);
    if(!arena->region) {
        ctx->free(arena);
        return NULL;
    }

    arena->size = size;
    arena->current = 0;
    arena->next = NULL;

    return arena;
}

struct shard_arena* shard_arena_init(const struct shard_context* ctx) {
    return _arena_init(ctx, PAGE_SIZE);
}

void* shard_arena_malloc(const struct shard_context* ctx, struct shard_arena* arena, size_t size) {
    struct shard_arena* last = arena;

    do {
        if(arena->size - arena->current >= size) {
            arena->current += size;
            return arena->region + (arena->current - size);
        }

        last = arena;
    } while((arena = arena->next) != NULL);

    size_t arena_size = size > PAGE_SIZE ? size : PAGE_SIZE;
    struct shard_arena* next = _arena_init(ctx, arena_size);

    last->next = next;
    next->current += size;
    return next->region;
}

void shard_arena_free(const struct shard_context* ctx, struct shard_arena* arena) {
    struct shard_arena* next, *last = arena;
    do {
        next = last->next;
        ctx->free(last->region);
        ctx->free(last);
        last = next;
    } while(next != NULL);
}

