#define _LIBSHARD_INTERNAL
#include <libshard.h>
#include <errno.h>

int shard_init_ext(struct shard_context* ctx, void* stack_base) {
    assert(ctx->malloc && ctx->realloc && ctx->free);
    ctx->ast = shard_arena_init(ctx);
    ctx->errors = (struct shard_errors){0};
    ctx->string_literals = (struct shard_string_list){0};
    ctx->include_dirs = (struct shard_string_list){0};
    ctx->ident_arena = shard_arena_init(ctx);
    shard_hashmap_init(ctx, &ctx->idents, 128);
    shard_hashmap_init(ctx, &ctx->open_sources, 16);

    ctx->gc = shard_gc_begin(ctx, stack_base);

    return 0;
}

void shard_deinit(struct shard_context* ctx) {
    for(struct shard_hashpair* pair = ctx->open_sources.pairs; pair < &ctx->open_sources.pairs[ctx->open_sources.alloc]; pair++) {
        if(!pair->key || !pair->value)
            continue;
        
        struct shard_open_source* source = pair->value;
        if(source->opened && source->auto_close) {
            source->source.close(&source->source);
            source->opened = false;
        }

        if(source->evaluated)
            shard_free_expr(ctx, &source->expr);

        if(source->auto_free)
            ctx->free(source);
    }

    shard_hashmap_free(ctx, &ctx->open_sources);

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

    shard_gc_end(ctx->gc);
}

void shard_include_dir(struct shard_context* ctx, char* path) {
    dynarr_append(ctx, &ctx->include_dirs, path);
}

struct shard_error* shard_get_errors(struct shard_context* ctx) {
    return ctx->errors.items;
}

size_t shard_get_num_errors(struct shard_context* ctx) {
    return ctx->errors.count;
}

void shard_remove_errors(struct shard_context* context) {
    context->errors.count = 0;
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

void shard_set_current_system(struct shard_context* ctx, const char* current_system) {
    ctx->current_system = current_system;
}

struct shard_open_source* shard_open(struct shard_context* ctx, const char* origin) {
    char* full_origin = ctx->realpath(origin, NULL);
    if(!full_origin) {
        // errno set by ctx->realpath
        return NULL;
    }

    struct shard_open_source* source = shard_hashmap_get(&ctx->open_sources, full_origin);
    if(source) {
        ctx->free(full_origin);
        return source;
    }

    source = ctx->malloc(sizeof(struct shard_open_source));
    memset(source, 0, sizeof(struct shard_open_source));

    int err = ctx->open(full_origin, &source->source, "r");
    if(err) {
        errno = err;
        ctx->free(full_origin);
        return NULL;
    }

    source->opened = true;
    source->auto_free = true;
    source->auto_close = true;

    err = shard_hashmap_put(ctx, &ctx->open_sources, full_origin, source);

    if(err) {
        ctx->free(full_origin);
        source->source.close(&source->source);
        ctx->free(source);
    }

    return err ? NULL : source;
}

int shard_register_open(struct shard_context* ctx, const char* origin, bool is_path, struct shard_open_source* source) {
    if(!source)
        return EINVAL;
    
    
    const char* full_origin = origin;
    if(is_path) {
        full_origin = ctx->realpath(origin, NULL);
        if(!full_origin) {
            // errno set by ctx->realpath
            return errno;
        }
    }

    bool just_opened = false;
    if(!source->opened) {
        int err = ctx->open(full_origin, &source->source, "r");
        if(err) {
            if(is_path)
                ctx->free((void*) full_origin);
            return err;
        }

        source->opened = true;
        source->auto_close = true;
        just_opened = true;
    }

    int err = shard_hashmap_put(ctx, &ctx->open_sources, full_origin, source);
    
    if(is_path && !just_opened)
        ctx->free((void*) full_origin);

    return err;
}

