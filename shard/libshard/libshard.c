#include <stdio.h>
#define _LIBSHARD_INTERNAL
#include <libshard.h>

#include <assert.h>
#include <string.h>

int shard_init(struct shard_context* context) {
    assert(context->malloc && context->realloc && context->free);
    context->idents = arena_init(context);
    context->errors = (struct shard_errors){0};
    context->string_literals = (struct shard_string_list){0};
    return 0;
}

void shard_deinit(struct shard_context* context) {
    arena_free(context, context->idents);

    for(size_t i = 0; i < context->errors.count; i++)
        if(context->errors.items[i].heap)
            context->free(context->errors.items[i].err);
    dynarr_free(context, &context->errors);

    for(size_t i = 0; i < context->string_literals.count; i++)
        context->free(context->string_literals.items[i]);
    dynarr_free(context, &context->string_literals);
}

int shard_eval(struct shard_context* context, struct shard_source* src, struct shard_value* result) {
    struct shard_expr expr;
    int err = shard_parse(context, src, &expr);
    if(err)
        goto finish;

    char buf[1024];
    shard_dump_expr(buf, sizeof(buf), &expr);
    printf("%s\n", buf);

    shard_free_expr(context, &expr);
finish:
    return context->errors.count;
}

struct shard_error* shard_get_errors(struct shard_context* context) {
    return context->errors.items;
}

void shard_free_expr(struct shard_context* ctx, struct shard_expr* expr) {
    if(!expr)
        return;

    switch(expr->type) {
        case SHARD_EXPR_STRING:
            ctx->free(expr->string);
            break;
        default:
    }
}

#define E(t) [SHARD_TOK_##t]

static const char* token_type_strings[_SHARD_TOK_LEN] = {
    E(EOF) = "<end of file>",
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

ptrdiff_t shard_dump_expr(char* dest, size_t n, const struct shard_expr* expr) {
    switch(expr->type) {
        case SHARD_EXPR_TRUE:
            return strncpy(dest, "true", n) - dest;
            break;
        case SHARD_EXPR_FALSE:
            return strncpy(dest, "false", n) - dest;
        case SHARD_EXPR_IDENT:
            return strncpy(dest, expr->ident, n) - dest;
        case SHARD_EXPR_STRING:
            return strncpy(dest, expr->string, n) - dest;
        case SHARD_EXPR_NUMBER:
            snprintf(dest, n, "%f", expr->number);
            return strlen(dest);
        default:
            return strncpy(dest, "<unknown>", n) - dest;
    }
}

