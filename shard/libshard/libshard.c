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
    ctx->include_dirs = (struct shard_string_list){0};
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

#define E(t) [SHARD_TOK_##t]

static const char* token_type_strings[_SHARD_TOK_LEN] = {
    E(EOF) = "<end of file>",
    E(IDENT) = "identifier",
    E(PATH) = "path literal",
    E(STRING) = "string literal",
    E(INT) = "integer literal",
    E(FLOAT) = "floating literal",
    E(LPAREN) = "(",
    E(RPAREN) = ")",
    E(LBRACKET) = "[",
    E(RBRACKET) = "]",
    E(LBRACE) = "{",
    E(RBRACE) = "}",
    E(ASSIGN) = "=",
    E(EQ) = "==",
    E(NE) = "!=",
    E(GT) = ">",
    E(GE) = ">=",
    E(LT) = "<",
    E(LE) = "<=",
    E(COLON) = ":",
    E(SEMICOLON) = ";",
    E(PERIOD) = ".",
    E(ELLIPSE) = "..",
    E(MERGE) = "//",
    E(CONCAT) = "++",
    E(QUESTIONMARK) = "?",
    E(EXCLAMATIONMARK) = "!",
    E(AT) = "@",
    E(ADD) = "+",
    E(SUB) = "-",
    E(MUL) = "*",
    E(DIV) = "/",
    E(LOGAND) = "&&",
    E(LOGOR) = "||",
    E(LOGIMPL) = "->",
    E(NULL) = "null",
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
        case SHARD_TOK_PATH:
            strncpy(dest, tok->value.string, n);
            break;
        case SHARD_TOK_FLOAT:
            snprintf(dest, n, "%f", tok->value.floating);
            break;
        case SHARD_TOK_INT:
            snprintf(dest, n, "%ld", tok->value.integer);
            break;
        default:
            if(token_type_strings[tok->type])
                strncpy(dest, token_type_strings[tok->type], n);
            else
                strncpy(dest, "<unknown>", n);
    }
}

#define BOP(name, _str) case SHARD_EXPR_##name:                     \
        shard_dump_expr(ctx, str, expr->binop.lhs);                 \
        dynarr_append_many(ctx, str, " " _str " ", LEN(_str) + 1);  \
        shard_dump_expr(ctx, str, expr->binop.rhs);                 \
        break

void shard_dump_expr(struct shard_context* ctx, struct shard_string* str, const struct shard_expr* expr) {
    dynarr_append(ctx, str, '(');
    
    switch(expr->type) {
        case SHARD_EXPR_NULL:
            dynarr_append_many(ctx, str, "null", 4);
            break;
        case SHARD_EXPR_TRUE:
            dynarr_append_many(ctx, str, "true", 4);
            break;
        case SHARD_EXPR_FALSE:
            dynarr_append_many(ctx, str, "false", 5);
            break;
        case SHARD_EXPR_IDENT:
            dynarr_append_many(ctx, str, expr->string, strlen(expr->string));
            break;
        case SHARD_EXPR_STRING:
            dynarr_append(ctx, str, '"');
            dynarr_append_many(ctx, str, expr->string, strlen(expr->string));
            dynarr_append(ctx, str, '"');
            break;
        case SHARD_EXPR_PATH:
            dynarr_append_many(ctx, str, expr->string, strlen(expr->string));
            break;
        case SHARD_EXPR_FLOAT: {
            char buf[32];
            snprintf(buf, 32, "%f", expr->floating);
            dynarr_append_many(ctx, str, buf, strlen(buf));
            break;
        }
        case SHARD_EXPR_INT: {
            char buf[32];
            snprintf(buf, 32, "%ld", expr->integer);
            dynarr_append_many(ctx, str, buf, strlen(buf));
            break;
        }
        case SHARD_EXPR_WITH:
            dynarr_append_many(ctx, str, "with ", 5);
            shard_dump_expr(ctx, str, expr->binop.lhs);
            dynarr_append_many(ctx, str, "; ", 2);
            shard_dump_expr(ctx, str, expr->binop.rhs);
            break;
        case SHARD_EXPR_ASSERT:
            dynarr_append_many(ctx, str, "assert ", 7);
            shard_dump_expr(ctx, str, expr->binop.lhs);
            dynarr_append_many(ctx, str, "; ", 2);
            shard_dump_expr(ctx, str, expr->binop.rhs);
            break;
        case SHARD_EXPR_NEGATE:
            dynarr_append(ctx, str, '-');
            shard_dump_expr(ctx, str, expr->unaryop.expr);
            break;
        case SHARD_EXPR_NOT:
            dynarr_append(ctx, str, '-');
            shard_dump_expr(ctx, str, expr->unaryop.expr);
            break;
        case SHARD_EXPR_TERNARY:
            dynarr_append_many(ctx, str, "if ", 3);
            shard_dump_expr(ctx, str, expr->ternary.cond);
            dynarr_append_many(ctx, str, " then ", 6);
            shard_dump_expr(ctx, str, expr->ternary.if_branch);
            dynarr_append_many(ctx, str, " else ", 6);
            shard_dump_expr(ctx, str, expr->ternary.else_branch);
            break;
        BOP(ADD, "+");
        BOP(SUB, "-");
        BOP(MUL, "*");
        BOP(DIV, "/");
        BOP(EQ, "==");
        BOP(NE, "!=");
        BOP(GT, ">");
        BOP(GE, ">=");
        BOP(LT, "<");
        BOP(LE, "<=");
        BOP(MERGE, "//");
        BOP(CONCAT, "++");
        BOP(LOGOR, "||");
        BOP(LOGAND, "&&");
        BOP(LOGIMPL, "->");
        case SHARD_EXPR_CALL:
            shard_dump_expr(ctx, str, expr->binop.lhs);
            dynarr_append(ctx, str, ' ');
            shard_dump_expr(ctx, str, expr->binop.rhs);
            break;
        case SHARD_EXPR_LIST:
            dynarr_append(ctx, str, '[');
            dynarr_append(ctx, str, ' ');
            for(size_t i = 0; i < expr->list.elems.count; i++) {
                shard_dump_expr(ctx, str, &expr->list.elems.items[i]);
                dynarr_append(ctx, str, ' ');
            }
            dynarr_append(ctx, str, ']');
            break;
        case SHARD_EXPR_ATTR_SEL:
            shard_dump_expr(ctx, str, expr->attr_sel.set);
            dynarr_append_many(ctx, str, " . <attr path>", 14);
            if(expr->attr_sel.default_value) {
                dynarr_append_many(ctx, str, " or ", 4);
                shard_dump_expr(ctx, str, expr->attr_sel.default_value);
            }
            break;
        case SHARD_EXPR_ATTR_TEST:
            shard_dump_expr(ctx, str, expr->attr_test.set);
            dynarr_append_many(ctx, str, " . <attr path>", 14);
            break;
        default:
            dynarr_append_many(ctx, str, "<unknown>", 9);
    }

    dynarr_append(ctx, str, ')');
}

#undef BOP

