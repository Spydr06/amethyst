#include "parse.h"
#include "libshard.h"
#include "token.h"
#include "shell.h"

#include <assert.h>
#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <memory.h>

#define _LIBSHARD_INTERNAL
#include <libshard-internal.h>

enum precedence {
    PREC_LOWEST,

    PREC_LOGAND,
    PREC_LOGOR,
    PREC_CALL,
    PREC_UNARY,

    PREC_HIGHEST
};

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

static enum precedence get_precedence(enum shell_token_type type) {
    switch(type) {
        case SH_TOK_EOF:
            return PREC_LOWEST;
        default:
            return PREC_HIGHEST;
    }
}

static int parse_prefix_expr(struct shell_parser* p, struct shard_expr* expr) {
    expr->loc = p->token.loc;

    switch(p->token.type) {
        case SH_TOK_TEXT:
            expr->type = SHARD_EXPR_STRING;
            expr->string = shard_gc_strdup(shell.shard.gc, p->token.value.items, p->token.value.count);
            advance(p);
            return 0;

        default:
            unexpected_token_error(p, "expression");
            return EINVAL;
    }
}

static int parse_infix_expr(struct shell_parser* p, struct shard_expr* expr, struct shard_expr* left) {
    switch(p->token.type) {
        default:
            assert(!"unimplemented");
    }    
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

