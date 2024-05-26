#define _LIBSHARD_INTERNAL
#include <libshard.h>

#include <stdio.h>

int shard_init(struct shard_context* ctx) {
    assert(ctx->malloc && ctx->realloc && ctx->free);
    ctx->ast = shard_arena_init(ctx);
    ctx->errors = (struct shard_errors){0};
    ctx->string_literals = (struct shard_string_list){0};
    ctx->include_dirs = (struct shard_string_list){0};
    ctx->ident_arena = shard_arena_init(ctx);
    shard_hashmap_init(ctx, &ctx->idents, 128);
    return 0;
}

void shard_deinit(struct shard_context* ctx) {
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

int shard_eval(struct shard_context* ctx, struct shard_source* src, struct shard_value* result) {
    struct shard_expr expr;
    int err = shard_parse(ctx, src, &expr);
    if(err)
        goto finish;

    struct shard_string str = {0};
    shard_dump_expr(ctx, &str, &expr);
    dynarr_append(ctx, &str, '\0');

    printf("%s\n", str.items);

    dynarr_free(ctx, &str);

    shard_free_expr(ctx, &expr);
finish:
    return ctx->errors.count;
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

