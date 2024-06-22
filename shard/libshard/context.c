#define _LIBSHARD_INTERNAL
#include <libshard.h>

int shard_init_ext(struct shard_context* ctx, void* stack_base) {
    assert(ctx->malloc && ctx->realloc && ctx->free);
    ctx->ast = shard_arena_init(ctx);
    ctx->errors = (struct shard_errors){0};
    ctx->string_literals = (struct shard_string_list){0};
    ctx->include_dirs = (struct shard_string_list){0};
    ctx->ident_arena = shard_arena_init(ctx);
    shard_hashmap_init(ctx, &ctx->idents, 128);

    shard_gc_begin(&ctx->gc, ctx, stack_base);

    shard_get_builtins(ctx, &ctx->builtin_scope);

    return 0;
}

void shard_deinit(struct shard_context* ctx) {
    shard_gc_end(&ctx->gc);

    shard_arena_free(ctx, ctx->ast);

    for(size_t i = 0; i < ctx->errors.count; i++)
        if(ctx->errors.items[i].heap)
            ctx->free(ctx->errors.items[i].err);
    dynarr_free(ctx, &ctx->errors);

    for(size_t i = 0; i < ctx->string_literals.count; i++)
        ctx->free(ctx->string_literals.items[i]);
    dynarr_free(ctx, &ctx->string_literals);

    dynarr_free(ctx, &ctx->include_dirs);

    shard_arena_free(ctx, ctx->ident_arena);
    shard_hashmap_free(ctx, &ctx->idents);
}

void shard_include_dir(struct shard_context* ctx, char* path) {
    dynarr_append(ctx, &ctx->include_dirs, path);
}

struct shard_error* shard_get_errors(struct shard_context* ctx) {
    return ctx->errors.items;
}

shard_ident_t shard_get_ident(struct shard_context* ctx, const char* key) {
    char* ident = shard_hashmap_get(&ctx->idents, key);
    if(ident)
        return ident;

    size_t ident_len = strlen(key);
    ident = shard_arena_malloc(ctx, ctx->ident_arena, ident_len + 1);
    strcpy(ident, key);

    shard_hashmap_put(ctx, &ctx->idents, ident, ident);
    return ident;
}

