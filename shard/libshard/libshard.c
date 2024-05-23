#include <stdio.h>
#define _LIBSHARD_INTERNAL
#include <libshard.h>

#include <assert.h>
#include <string.h>

int shard_init(struct shard_context* ctx) {
    assert(ctx->malloc && ctx->realloc && ctx->free);
    ctx->idents = arena_init(ctx);
    ctx->ast = arena_init(ctx);
    ctx->errors = (struct shard_errors){0};
    ctx->string_literals = (struct shard_string_list){0};
    return 0;
}

void shard_deinit(struct shard_context* ctx) {
    arena_free(ctx, ctx->idents);
    arena_free(ctx, ctx->ast);

    for(size_t i = 0; i < ctx->errors.count; i++)
        if(ctx->errors.items[i].heap)
            ctx->free(ctx->errors.items[i].err);
    dynarr_free(ctx, &ctx->errors);

    for(size_t i = 0; i < ctx->string_literals.count; i++)
        ctx->free(ctx->string_literals.items[i]);
    dynarr_free(ctx, &ctx->string_literals);
}

int shard_eval(struct shard_context* ctx, struct shard_source* src, struct shard_value* result) {
    struct shard_expr expr;
    int err = shard_parse(ctx, src, &expr);
    if(err)
        goto finish;

    char buf[BUFSIZ];
    shard_dump_expr(buf, sizeof(buf), &expr);
    printf("%s\n", buf);

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
        default:
    }
}

#define E(t) [SHARD_TOK_##t]

static const char* token_type_strings[_SHARD_TOK_LEN] = {
    E(EOF) = "<end of file>",
    E(IDENT) = "identifier",
    E(STRING) = "string literal",
    E(NUMBER) = "number literal",
    E(LPAREN) = "(",
    E(RPAREN) = ")",
    E(LBRACKET) = "[",
    E(RBRACKET) = "]",
    E(LBRACE) = "{",
    E(RBRACE) = "}",
    E(ASSIGN) = "=",
    E(EQ) = "==",
    E(NE) = "!=",
    E(COLON) = ":",
    E(SEMICOLON) = ";",
    E(PERIOD) = ".",
    E(ELLIPSE) = "..",
    E(MERGE) = "++",
    E(QUESTIONMARK) = "?",
    E(EXCLAMATIONMARK) = "!",
    E(AT) = "@",
    E(ADD) = "+",
    E(SUB) = "-",
    E(TRUE) = "true",
    E(FALSE) = "false",
    E(REC) = "rec",
    E(OR) = "or",
    E(IF) = "if",
    E(THEN) = "then",
    E(ELSE) = "else",
    E(ASSERT) = "assert",
    E(LET) = "let",
    E(IN) = "in",
    E(WITH) = "with"
};

#undef E

const char* shard_token_type_to_str(enum shard_token_type token_type) {
    return token_type < 0 ? "<error token>" : token_type_strings[token_type];
}

void shard_dump_token(char* dest, size_t n, const struct shard_token* tok) {
    switch(tok->type) {
        case SHARD_TOK_ERR:
            snprintf(dest, n, "<error: %s>", tok->value.string);
            break;
        case SHARD_TOK_IDENT:
            strncpy(dest, tok->value.string, n);
            break;
        case SHARD_TOK_STRING:
            snprintf(dest, n, "\"%s\"", tok->value.string);
            break;
        case SHARD_TOK_NUMBER:
            snprintf(dest, n, "%f", tok->value.number);
            break;
        default:
            if(token_type_strings[tok->type])
                strncpy(dest, token_type_strings[tok->type], n);
            else
                strncpy(dest, "<unknown>", n);
    }
}

size_t shard_dump_expr(char* dest, size_t n, const struct shard_expr* expr) {
    size_t off = 0;
    switch(expr->type) {
        case SHARD_EXPR_TRUE:
            return shard_stpncpy(dest, "true", n) - dest;
            break;
        case SHARD_EXPR_FALSE:
            return shard_stpncpy(dest, "false", n) - dest;
        case SHARD_EXPR_IDENT:
            return shard_stpncpy(dest, expr->ident, n) - dest;
        case SHARD_EXPR_STRING:
            snprintf(dest, n, "\"%s\"", expr->string);
            return strlen(dest);
        case SHARD_EXPR_NUMBER:
            snprintf(dest, n, "%f", expr->number);
            return strlen(dest);
        case SHARD_EXPR_WITH:
            off = shard_stpncpy(dest, "with ", n) - dest;
            off += shard_dump_expr(dest + off, n - off, expr->binop.lhs);
            off = shard_stpncpy(dest + off, "; ", n - off) - dest;
            off += shard_dump_expr(dest + off, n - off, expr->binop.rhs);
            return off;
        case SHARD_EXPR_NEGATE:
            off = shard_stpncpy(dest, "-", n) - dest;
            return off + shard_dump_expr(dest + off, n - off, expr->unaryop.expr);
        case SHARD_EXPR_NOT:
            off = shard_stpncpy(dest, "!", n) - dest;
            return off + shard_dump_expr(dest + off, n - off, expr->unaryop.expr);
        case SHARD_EXPR_TERNARY:
            off = shard_stpncpy(dest, "if ", n) - dest;
            off += shard_dump_expr(dest + off, n - off, expr->ternary.cond);
            off = shard_stpncpy(dest + off, " then ", n - off) - dest;
            off += shard_dump_expr(dest + off, n - off, expr->ternary.if_branch);
            off = shard_stpncpy(dest + off, " else ", n - off) - dest;
            off += shard_dump_expr(dest + off, n - off, expr->ternary.else_branch);
            return off;
        case SHARD_EXPR_ADD:
        case SHARD_EXPR_SUB:
            off += shard_dump_expr(dest, n, expr->binop.lhs);
            off = shard_stpncpy(dest + off, expr->type == SHARD_EXPR_ADD ? " + " : " - ", n - off) - dest;
            off += shard_dump_expr(dest + off, n - off, expr->binop.rhs);
            return off;
        default:
            return strncpy(dest, "<unknown>", n) - dest;
    }
}

