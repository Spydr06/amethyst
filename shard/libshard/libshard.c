#include <stdio.h>
#define _LIBSHARD_INTERNAL
#include <libshard.h>

#include <assert.h>

int shard_init(struct shard_context* ctx) {
    assert(ctx->malloc && ctx->realloc && ctx->free);
    ctx->idents = shard_arena_init(ctx);
    ctx->ast = shard_arena_init(ctx);
    ctx->errors = (struct shard_errors){0};
    ctx->string_literals = (struct shard_string_list){0};
    ctx->include_dirs = (struct shard_string_list){0};
    return 0;
}

void shard_deinit(struct shard_context* ctx) {
    shard_arena_free(ctx, ctx->idents);
    shard_arena_free(ctx, ctx->ast);

    for(size_t i = 0; i < ctx->errors.count; i++)
        if(ctx->errors.items[i].heap)
            ctx->free(ctx->errors.items[i].err);
    dynarr_free(ctx, &ctx->errors);

    for(size_t i = 0; i < ctx->string_literals.count; i++)
        ctx->free(ctx->string_literals.items[i]);
    dynarr_free(ctx, &ctx->string_literals);

    dynarr_free(ctx, &ctx->include_dirs);
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

void shard_free_expr(struct shard_context* ctx, struct shard_expr* expr) {
    if(!expr)
        return;

    switch(expr->type) {
        case SHARD_EXPR_STRING:
            ctx->free(expr->string);
            break;
        case SHARD_EXPR_WITH:
        case SHARD_EXPR_ASSERT:
        case SHARD_EXPR_ADD:
        case SHARD_EXPR_SUB:
        case SHARD_EXPR_MUL:
        case SHARD_EXPR_DIV:
        case SHARD_EXPR_CONCAT:
        case SHARD_EXPR_MERGE:
        case SHARD_EXPR_EQ:
        case SHARD_EXPR_NE:
        case SHARD_EXPR_GT:
        case SHARD_EXPR_GE:
        case SHARD_EXPR_LT:
        case SHARD_EXPR_LE:
        case SHARD_EXPR_LOGAND:
        case SHARD_EXPR_LOGOR:
        case SHARD_EXPR_LOGIMPL:
        case SHARD_EXPR_CALL:
            shard_free_expr(ctx, expr->binop.lhs);
            shard_free_expr(ctx, expr->binop.rhs);
            break;
        case SHARD_EXPR_NOT:
        case SHARD_EXPR_NEGATE:
            shard_free_expr(ctx, expr->unaryop.expr);
            break;
        case SHARD_EXPR_TERNARY:
            shard_free_expr(ctx, expr->ternary.cond);
            shard_free_expr(ctx, expr->ternary.if_branch);
            shard_free_expr(ctx, expr->ternary.else_branch);
            break;
        case SHARD_EXPR_LIST:
            for(size_t i = 0; i < expr->list.elems.count; i++)
                shard_free_expr(ctx, &expr->list.elems.items[i]);
            dynarr_free(ctx, &expr->list.elems);
            break;
        case SHARD_EXPR_SET:
            for(size_t i = 0; i < expr->set.attrs.alloc; i++)
                if(expr->set.attrs.pairs[i].value)
                    shard_free_expr(ctx, expr->set.attrs.pairs[i].value);
            shard_hashmap_free(ctx, &expr->set.attrs);
            break;
        case SHARD_EXPR_ATTR_TEST:
            shard_free_expr(ctx, expr->attr_test.set);
            dynarr_free(ctx, &expr->attr_test.path);
            break;
        case SHARD_EXPR_ATTR_SEL:
            shard_free_expr(ctx, expr->attr_sel.set);
            if(expr->attr_sel.default_value)
                shard_free_expr(ctx, expr->attr_sel.default_value);
            dynarr_free(ctx, &expr->attr_sel.path);
            break;
        default:
    }
}

void shard_attr_path_init(struct shard_context* ctx, struct shard_attr_path* path) {
    path->items = ctx->malloc(sizeof(shard_ident_t));
    path->count = 0;
    path->capacity = 1;
}

