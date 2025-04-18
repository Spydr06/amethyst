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
    
    PREC_REDIRECT,
    PREC_PIPE,
    PREC_ASYNC,

    PREC_CALL,
    PREC_LIST,

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
    if(p->token.type != SH_TOK_TEXT || p->token.escaped)
        return false;

    return strcmp(p->token.value.items, keyword) == 0;
}

static bool token_terminates_expr(enum shell_token_type type) {
    switch(type) {
        case SH_TOK_RPAREN:
        case SH_TOK_EOF:
            return true;
        default:
            return false;
    }
}

static enum precedence get_precedence(enum shell_token_type type) {
    switch(type) {
        case SH_TOK_SEMICOLON:
            return PREC_SEQUENCE;
        case SH_TOK_AMPERSAND:
            return PREC_ASYNC;
        case SH_TOK_REDIRECT:
        case SH_TOK_REDIRECT_APPEND:
            return PREC_REDIRECT;
        case SH_TOK_PIPE:
            return PREC_PIPE;
        case SH_TOK_AND:
            return PREC_LOGAND;
        case SH_TOK_OR:
            return PREC_LOGOR;
        default:
            return token_terminates_expr(type) ? PREC_LOWEST : PREC_CALL;
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

static int parse_list(struct shell_parser* p, struct shard_expr* expr) {
    expr->type = SHARD_EXPR_LIST;
    expr->loc = p->token.loc;

    advance(p);

    struct shard_expr_list elems = {0};

    struct shard_expr elem;
    while(!is_keyword(p, "]") && p->token.type != SH_TOK_EOF) {
        int err = parse_expr(p, &elem, PREC_LIST);
        if(err)
            return err;

        dynarr_gc_append(shell.shard.gc, &elems, elem);
    }

    if(p->token.type == SH_TOK_EOF) {
        unexpected_token_error(p, "closing `]` for list literal");
        return EINVAL;
    }
    else {
        advance(p);
    }

    expr->list.elems = elems;
    return 0;
}

static int parse_prefix_expr(struct shell_parser* p, struct shard_expr* expr) {
    expr->loc = p->token.loc;

    switch(p->token.type) {
        case SH_TOK_VAR:
            return parse_var(p, expr);
        case SH_TOK_TEXT:
            if(is_keyword(p, "with"))
                return parse_with(p, expr);
            else if(is_keyword(p, "["))
                return parse_list(p, expr);

            const char* text = p->token.value.items;
            size_t text_size = p->token.value.count;
            expr->type = text[0] == '~' ? SHARD_EXPR_PATH : SHARD_EXPR_STRING;
            expr->string = shard_gc_strdup(shell.shard.gc, text, text_size);
            advance(p);
            return 0;
        case SH_TOK_LPAREN:
            advance(p);
            int err = parse_expr(p, expr, PREC_LOWEST);
            if(err)
                return err;
            return consume(p, SH_TOK_RPAREN);
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

static enum shell_iostream parse_iostream(struct shell_parser* p) {
    assert(p->token.type == SH_TOK_REDIRECT || p->token.type == SH_TOK_REDIRECT_APPEND || p->token.type == SH_TOK_PIPE);

    if(p->token.value.count == 0)
        return SHELL_STDOUT;

    if(strcmp(p->token.value.items, "out") == 0)
        return SHELL_STDOUT;
    if(strcmp(p->token.value.items, "err") == 0)
        return SHELL_STDERR;
    if(strcmp(p->token.value.items, "all") == 0)
        return SHELL_STDOUT | SHELL_STDERR;

    parse_errorf(p, p->token.loc, "unknown stream `%s`, expect `out`, `err` or `all`", p->token.value.items);
    return -EINVAL;
}

static inline struct shard_expr* make_integer_literal(int64_t value, struct shard_location loc) {
    struct shard_expr* lit = shard_gc_malloc(shell.shard.gc, sizeof(struct shard_expr));
    lit->type = SHARD_EXPR_INT;
    lit->loc = loc;
    lit->integer = value;
    return lit;
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

    bool wait = true;
    struct shard_location wait_loc;
    
    enum shell_iostream pipe_to = 0;
    enum shell_iostream redirect_to = 0;

    struct shard_expr* piped_call = NULL;
    struct shard_location pipe_loc;

    struct shard_expr* redirect_expr = NULL;
    struct shard_location redirect_loc;

    switch(p->token.type) {
        case SH_TOK_AMPERSAND:
            wait_loc = p->token.loc;
            wait = false;
            advance(p);
            break;
        case SH_TOK_PIPE:        
            if((pipe_to = parse_iostream(p)) < 0)
                return -pipe_to;
            pipe_loc = p->token.loc;
            advance(p);
            piped_call = shard_gc_malloc(shell.shard.gc, sizeof(struct shard_expr));
            if((err = parse_call(p, piped_call)))
                return err;
            break;
        case SH_TOK_REDIRECT:
            if((redirect_to = parse_iostream(p)) < 0)
                return -pipe_to;
            redirect_loc = p->token.loc;
            advance(p);
            redirect_expr = shard_gc_malloc(shell.shard.gc, sizeof(struct shard_expr));
            if((err = parse_prefix_expr(p, redirect_expr)))
                return err;
            break;
        default:
            break;
    }

    struct shard_expr* create_process = shard_gc_malloc(shell.shard.gc, sizeof(struct shard_expr));
    create_process->type = SHARD_EXPR_BUILTIN;
    create_process->loc = loc;
    create_process->builtin.builtin = &shell_builtin_createProcess;

    struct shard_expr* create_process_call = shard_gc_malloc(shell.shard.gc, sizeof(struct shard_expr));
    create_process_call->loc = loc;
    create_process_call->type = SHARD_EXPR_CALL;
    create_process_call->binop.lhs = create_process;
    create_process_call->binop.rhs = arg_list;

    struct shard_expr* pid_expr = create_process_call;

    if(redirect_expr) {
        struct shard_expr* redirect = shard_gc_malloc(shell.shard.gc, sizeof(struct shard_expr));
        redirect->type = SHARD_EXPR_BUILTIN;
        redirect->loc = redirect_loc;
        redirect->builtin.builtin = &shell_builtin_redirect;

        struct shard_expr* redirect_call = shard_gc_malloc(shell.shard.gc, sizeof(struct shard_expr) * 3);
        redirect_call[0].type = SHARD_EXPR_CALL;
        redirect_call[0].loc = redirect_loc;
        redirect_call[0].binop.lhs = redirect;
        redirect_call[0].binop.rhs = make_integer_literal((int64_t) redirect_to, redirect_loc);

        redirect_call[1].type = SHARD_EXPR_CALL;
        redirect_call[1].loc = redirect_loc;
        redirect_call[1].binop.lhs = &redirect_call[0];
        redirect_call[1].binop.rhs = redirect_expr;

        redirect_call[2].type = SHARD_EXPR_CALL;
        redirect_call[2].loc = redirect_loc;
        redirect_call[2].binop.lhs = &redirect_call[1];
        redirect_call[2].binop.rhs = pid_expr;

        pid_expr = &redirect_call[2];
    }

    if(piped_call) {
        struct shard_expr* pipe = shard_gc_malloc(shell.shard.gc, sizeof(struct shard_expr));
        pipe->type = SHARD_EXPR_BUILTIN;
        pipe->loc = pipe_loc;
        pipe->builtin.builtin = &shell_builtin_pipe;

        struct shard_expr* pipe_call = shard_gc_malloc(shell.shard.gc, sizeof(struct shard_expr) * 3);
        pipe_call[0].type = SHARD_EXPR_CALL;
        pipe_call[0].loc = pipe_loc;
        pipe_call[0].binop.lhs = pipe;
        pipe_call[0].binop.rhs = make_integer_literal((int64_t) pipe_to, pipe_loc);
        
        pipe_call[1].type = SHARD_EXPR_CALL;
        pipe_call[1].loc = pipe_loc;
        pipe_call[1].binop.lhs = &pipe_call[0];
        pipe_call[1].binop.rhs = pipe_call;

        pipe_call[2].type = SHARD_EXPR_CALL;
        pipe_call[2].loc = pipe_loc;
        pipe_call[2].binop.lhs = &pipe_call[1];
        pipe_call[2].binop.rhs = pid_expr;

        pid_expr = &pipe_call[2];
    }

    struct shard_expr* finalizing_builtin = shard_gc_malloc(shell.shard.gc, sizeof(struct shard_expr));
    finalizing_builtin->type = SHARD_EXPR_BUILTIN;

    if(wait) {
        finalizing_builtin->loc = loc;
        finalizing_builtin->builtin.builtin = &shell_builtin_waitProcess;
    }
    else {
        finalizing_builtin->loc = wait_loc;
        finalizing_builtin->builtin.builtin = &shell_builtin_asyncProcess;
    }

    expr->type = SHARD_EXPR_CALL;
    expr->loc = loc;
    expr->binop.lhs = finalizing_builtin;
    expr->binop.rhs = pid_expr;

    return 0;
}

static int parse_expr(struct shell_parser* p, struct shard_expr* expr, enum precedence prec) {
    struct shard_expr tmp;
    
    int err;
    if(prec <= PREC_CALL)
        err = parse_call(p, expr);
    else
        err = parse_prefix_expr(p, expr);

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

