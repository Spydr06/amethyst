#include <stdio.h>
#define _LIBSHARD_INTERNAL
#include <libshard.h>

static void shard_free_pattern(struct shard_context* ctx, struct shard_pattern* pattern) {
    shard_free_expr(ctx, pattern->constant);
    shard_free_expr(ctx, pattern->condition);
    for(size_t i = 0; i < pattern->attrs.count; i++) {
        struct shard_binding attr = pattern->attrs.items[i];
        if(attr.value)
            shard_free_expr(ctx, attr.value);
    }
    dynarr_free(ctx, &pattern->attrs);
}

void shard_free_expr(struct shard_context* ctx, struct shard_expr* expr) {
    if(!expr)
        return;

    switch(expr->type) {
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
        case SHARD_EXPR_LCOMPOSE:
        case SHARD_EXPR_RCOMPOSE:
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
        case SHARD_EXPR_FUNCTION:
            shard_free_pattern(ctx, expr->func.arg);
            shard_free_expr(ctx, expr->func.body);
            break;
        case SHARD_EXPR_LET:
            for(size_t i = 0; i < expr->let.bindings.count; i++)
                shard_free_expr(ctx, expr->let.bindings.items[i].value);
            dynarr_free(ctx, &expr->let.bindings);
            shard_free_expr(ctx, expr->let.expr);
            break;
        case SHARD_EXPR_CASE_OF:
            shard_free_expr(ctx, expr->case_of.cond);

            for(size_t i = 0; i < expr->case_of.branches.count; i++)
                shard_free_expr(ctx, &expr->case_of.branches.items[i]);
            dynarr_free(ctx, &expr->case_of.branches);
            
            for(size_t i = 0; i < expr->case_of.patterns.count; i++)
                shard_free_pattern(ctx, &expr->case_of.patterns.items[i]);
            dynarr_free(ctx, &expr->case_of.patterns);
            break;
        case SHARD_EXPR_INTERPOLATED_STRING:
        case SHARD_EXPR_INTERPOLATED_PATH:
            for(size_t i = 0; i < expr->interpolated.exprs.count; i++)
                shard_free_expr(ctx, &expr->interpolated.exprs.items[i]);
            dynarr_free(ctx, &expr->interpolated.exprs);
            dynarr_free(ctx, &expr->interpolated.strings);
            break;
        default:
            break;
    }
}

void shard_attr_path_gc_init(struct shard_gc* gc, struct shard_attr_path* path) {
    path->items = shard_gc_malloc(gc, sizeof(shard_ident_t));
    path->count = 0;
    path->capacity = 1;
}


void shard_attr_path_init(struct shard_context* ctx, struct shard_attr_path* path) {
    path->items = ctx->malloc(sizeof(shard_ident_t));
    path->count = 0;
    path->capacity = 1;
}

void shard_attr_path_append(struct shard_context* ctx, struct shard_attr_path* path, shard_ident_t ident) {
    dynarr_append(ctx, path, ident);
}

void shard_attr_path_gc_append(struct shard_gc* gc, struct shard_attr_path* path, shard_ident_t ident) {
    dynarr_gc_append(gc, path, ident);
}

