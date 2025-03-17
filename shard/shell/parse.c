#include "parse.h"
#include "libshard.h"
#include "token.h"
#include "shell.h"

#include <assert.h>
#include <errno.h>
#include <stdarg.h>
#include <stdlib.h>
#include <stdio.h>
#include <memory.h>

#define _LIBSHARD_INTERNAL
#include <libshard-internal.h>

enum precedence {
    PREC_LOWEST,

    PREC_SEQUENCE,
    PREC_LOGAND,
    PREC_LOGOR,
    
    PREC_ASYNC,

    PREC_CALL,
    PREC_UNARY,

    PREC_HIGHEST
};

static int parse_expr(struct shell_parser* p, struct shard_expr* expr, enum precedence prec);

static void __attribute__((format(printf, 3, 4)))
parse_errorf(struct shell_parser* p, struct shard_location loc, const char* fmt, ...) {
    if(loc.src->origin)
        fprintf(stderr, "%s:", loc.src->origin);

    fprintf(stderr, "%u:%u: ", loc.line, loc.column);

    va_list ap;
    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    va_end(ap);

    fprintf(stderr, "\n");
}

static void unexpected_token_error(struct shell_parser* p, const char* expected) {
    struct shard_string token_str = {0};
    shell_token_to_string(&shell.shard, &p->token, &token_str);
    shard_string_push(&shell.shard, &token_str, '\0');

    parse_errorf(p, p->token.loc, "unexpected token `%s`, expected %s", token_str.items, expected);

    shard_string_free(&shell.shard, &token_str);
}

static void advance(struct shell_parser* p) {

    if(p->token.type != SH_TOK_EOF)
        lex_next(&p->l, &p->token);

    if(p->token.type == SH_TOK_ERROR) {
        parse_errorf(p, p->token.loc, "%s", p->token.value.items);
    }

    // printf("%d\n", p->token.type);
}

static int consume(struct shell_parser* p, enum shell_token_type type) {
    if(p->token.type != type) {
        struct shard_string token_str = {0};
        shell_token_to_string(&shell.shard, &p->token, &token_str);
        unexpected_token_error(p, token_str.items);
        shard_string_free(&shell.shard, &token_str);
        return EINVAL;
    }

    advance(p);
    return 0;
}

static bool is_keyword(struct shell_parser* p, const char* keyword) {
    if(p->token.type != SH_TOK_TEXT)
        return false;

    return strcmp(p->token.value.items, keyword) == 0;
}

static enum precedence get_precedence(enum shell_token_type type) {
    switch(type) {
        case SH_TOK_SEMICOLON:
            return PREC_SEQUENCE;
        case SH_TOK_AMPERSAND:
            return PREC_ASYNC;
        case SH_TOK_AND:
            return PREC_LOGAND;
        case SH_TOK_OR:
            return PREC_LOGOR;
        case SH_TOK_EOF:
            return PREC_LOWEST;
        default:
            return PREC_HIGHEST;
    }
}

static int parse_var(struct shell_parser* p, struct shard_expr* expr) {
    char* ident = strndup(p->token.value.items, p->token.value.count);
    char* attr = strtok(ident, ".");

    expr->type = SHARD_EXPR_IDENT;
    expr->loc = p->token.loc;
    expr->ident = shard_get_ident(&shell.shard, attr);
    
    if(!(attr = strtok(NULL, "."))) {
        advance(p);
        free(ident);
        return 0;
    }

    struct shard_expr* set = shard_gc_malloc(shell.shard.gc, sizeof(struct shard_expr));
    memcpy(set, expr, sizeof(struct shard_expr));

    expr->type = SHARD_EXPR_ATTR_SEL;
    expr->attr_sel.set = set;
    expr->attr_sel.default_value = NULL;
    
    shard_attr_path_gc_init(shell.shard.gc, &expr->attr_sel.path);

    do {
        shard_attr_path_gc_append(shell.shard.gc, &expr->attr_sel.path, shard_get_ident(&shell.shard, attr));
    } while((attr = strtok(NULL, ".")));

    advance(p);

    free(ident);
    return 0;
}

static int parse_with(struct shell_parser* p, struct shard_expr* expr) {
    advance(p);

    expr->type = SHARD_EXPR_WITH;
    expr->binop.lhs = shard_gc_malloc(shell.shard.gc, sizeof(struct shard_expr));
    expr->binop.rhs = shard_gc_malloc(shell.shard.gc, sizeof(struct shard_expr));

    int err = parse_expr(p, expr->binop.lhs, PREC_SEQUENCE);
    if(err)
        return err;

    err = consume(p, SH_TOK_SEMICOLON);
    if(err)
        return err;

    return parse_expr(p, expr->binop.rhs, PREC_LOWEST);
}

static int parse_prefix_expr(struct shell_parser* p, struct shard_expr* expr) {
    expr->loc = p->token.loc;

    switch(p->token.type) {
        case SH_TOK_VAR:
            return parse_var(p, expr);
        case SH_TOK_TEXT:
            if(is_keyword(p, "with"))
                return parse_with(p, expr);

            const char* text = p->token.value.items;
            size_t text_size = p->token.value.count;
            expr->type = text[0] == '~' ? SHARD_EXPR_PATH : SHARD_EXPR_STRING;
            expr->string = shard_gc_strdup(shell.shard.gc, text, text_size);
            advance(p);
            return 0;

        default:
            unexpected_token_error(p, "expression");
            return EINVAL;
    }
}

static int parse_wrapped_binop(struct shell_parser* p, struct shard_expr* expr, struct shard_expr* left, struct shard_builtin* wrapper_builtin) {
    struct shard_location loc = p->token.loc;
    enum precedence prec = get_precedence(p->token.type);
    advance(p);

    struct shard_expr* lhs = shard_gc_malloc(shell.shard.gc, sizeof(struct shard_expr));
    memcpy(lhs, left, sizeof(struct shard_expr));

    struct shard_expr* wrapper = shard_gc_malloc(shell.shard.gc, sizeof(struct shard_expr));
    wrapper->type = SHARD_EXPR_BUILTIN;
    wrapper->loc = loc;
    wrapper->builtin.builtin = wrapper_builtin;

    struct shard_expr* inner_call = shard_gc_malloc(shell.shard.gc, sizeof(struct shard_expr));
    inner_call->type = SHARD_EXPR_CALL;
    inner_call->loc = loc;
    inner_call->binop.lhs = wrapper;
    inner_call->binop.rhs = lhs;

    struct shard_expr* rhs = shard_gc_malloc(shell.shard.gc, sizeof(struct shard_expr));
    int err = parse_expr(p, rhs, prec);
    if(err)
        return err;

    expr->type = SHARD_EXPR_CALL;
    expr->loc = loc;
    expr->binop.lhs = inner_call;
    expr->binop.rhs = rhs;
    return 0;
}

static int parse_infix_expr(struct shell_parser* p, struct shard_expr* expr, struct shard_expr* left) {
    switch(p->token.type) {
        case SH_TOK_AMPERSAND:
            assert(!"unimplemented");
            break;
        case SH_TOK_SEMICOLON:
            return parse_wrapped_binop(p, expr, left, &shard_builtin_seq);
        case SH_TOK_AND:
            return parse_wrapped_binop(p, expr, left, &shell_builtin_and);
        case SH_TOK_OR:
            return parse_wrapped_binop(p, expr, left, &shell_builtin_or);
        default:
            assert(!"unimplemented");
    }    
    return 0;
}

static int parse_call(struct shell_parser* p, struct shard_expr* expr) {
    struct shard_location loc = p->token.loc;
    struct shard_expr_list args = {0};

    int err;
    do {
        struct shard_expr arg;

        if((err = parse_prefix_expr(p, &arg)))
            return err;

        dynarr_append(&shell.shard, &args, arg);
    } while(get_precedence(p->token.type) >= PREC_CALL);

    struct shard_expr* arg_list = shard_gc_malloc(shell.shard.gc, sizeof(struct shard_expr));

    arg_list->type = SHARD_EXPR_LIST;
    arg_list->loc = loc;
    arg_list->list.elems = args;

    struct shard_expr* builtin_call_program = shard_gc_malloc(shell.shard.gc, sizeof(struct shard_expr));
    builtin_call_program->type = SHARD_EXPR_BUILTIN;
    builtin_call_program->loc = loc;
    builtin_call_program->builtin.builtin = &shell_builtin_callProgram;

    expr->type = SHARD_EXPR_CALL;
    expr->loc = loc;
    expr->binop.lhs = builtin_call_program;
    expr->binop.rhs = arg_list;

    return 0;
}

static int parse_expr(struct shell_parser* p, struct shard_expr* expr, enum precedence prec) {
    struct shard_expr tmp;

    int err = parse_call(p, expr);
    if(err)
        return err;

    while(prec < get_precedence(p->token.type)) {
        if((err = parse_infix_expr(p, &tmp, expr)))
            return err;
        *expr = tmp;
    }

    return err;
}

void shell_parser_init(struct shell_parser* parser, struct shard_source* source) {
    memset(parser, 0, sizeof(struct shell_parser));
    lex_init(&parser->l, source);

    advance(parser);
}

void shell_parser_free(struct shell_parser* parser) {
    lex_free(&parser->l);
}

int shell_parse_next(struct shell_parser* p, struct shard_expr* expr) {
    memset(expr, 0, sizeof(struct shard_expr));

    switch(p->token.type) {
        case SH_TOK_ERROR:
            parse_errorf(p, p->token.loc, "invalid token: %s", p->token.value.items);
            return EINVAL;
        case SH_TOK_EOF:
            return EOF;
        default:
            return parse_expr(p, expr, PREC_LOWEST);
    }

    assert(!"unreachable");
}

