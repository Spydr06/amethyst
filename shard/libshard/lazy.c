#define _LIBSHARD_INTERNAL
#include <libshard.h>

struct shard_lazy_value* shard_lazy(struct shard_context* ctx, struct shard_expr* lazy, struct shard_scope* scope) {
    struct shard_lazy_value* value = shard_gc_malloc(ctx->gc, sizeof(struct shard_lazy_value));
    value->evaluated = false;
    value->lazy = lazy;
    value->scope = scope;
    return value;
}

struct shard_lazy_value* shard_unlazy(struct shard_context* ctx, struct shard_value eval) {
    struct shard_lazy_value* value = shard_gc_malloc(ctx->gc, sizeof(struct shard_lazy_value));
    value->evaluated = true;
    value->eval = eval;
    return value;
}

