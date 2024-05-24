#define _LIBSHARD_INTERNAL
#include <libshard.h>

#include <errno.h>
#include <stdarg.h>
#include <stdio.h>

enum precedence {
    PREC_LOWEST,
    PREC_FUNCTION,
    PREC_ADDITION,
    PREC_MULTIPLICATION,
    PREC_UNARY,
    PREC_CALL,
    PREC_LIST,
    PREC_HIGHEST
};

struct parser {
    struct shard_context* ctx;
    struct shard_source* src;
    struct shard_token token;
};

static int advance(struct parser* p) {
    int err = shard_lex(p->ctx, p->src, &p->token);
    if(err) {
        dynarr_append(p->ctx, &p->ctx->errors, ((struct shard_error){
            .err = p->token.value.string,
            .loc = p->token.location,
            ._errno = err
        }));
    }

    return err;
}

static int errorf(struct parser* p, const char* fmt, ...) {
    char* buffer = p->ctx->malloc(256);
    
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buffer, 1024, fmt, ap);
    va_end(ap);

    dynarr_append(p->ctx, &p->ctx->errors, ((struct shard_error){
        .err = buffer,
        .heap = true,
        .loc = p->token.location,
        ._errno = EINVAL
    }));

    advance(p);
    return EINVAL;
}

static inline int any_err(int* errors, size_t len) {
    for(register size_t i = 0; i < len; i++)
         if(errors[i])
             return errors[i];
    return 0;
}

static int consume(struct parser* p, enum shard_token_type token_type) {
    if(p->token.type == token_type)
        return advance(p);

    if(p->token.type == SHARD_TOK_ERR)
        return EINVAL;
    return errorf(
        p,
        "unexpected token `%s`, expect token `%s`",
        shard_token_type_to_str(p->token.type),
        shard_token_type_to_str(token_type)
    );
}

static int parse_expr(struct parser* p, struct shard_expr* expr, enum precedence prec);

static int parse_escape_code(struct parser* p, const char* ptr, char* c) {
    switch(*ptr) {
        case 'e':
            *c = 33;
            break;
        case 'n':
            *c = '\n';
            break;
        case 'r':
            *c = '\r';
            break;
        case '\\':
            *c = '\\';
            break;
        case 'b':
            *c = '\b';
            break;
        case 't':
            *c = '\t';
            break;
        case '\'':
            *c = '\'';
            break;
        case '"':
            *c = '"';
            break;
        case 'f':
            *c = '\f';
            break;
        case ' ':
            *c = ' ';
            break;
        case '\t':
            *c = '\t';
            break;
        case '\v':
            *c = '\v';
            break;
        case '\n':
            *c = '\n';
            break;
        case '\r':
            *c = '\r';
            break;
        default:
            return errorf(p, "invalid escape code: `\\%c`", *ptr ? *ptr : '0');
    }
    return 0;
}

static int parse_string_lit(struct parser* p, struct shard_expr* expr) {
    struct shard_string str = {0};

    int err = 0;
    for(const char* ptr = p->token.value.string; *ptr; ptr++) {
        switch(*ptr) {
            case '\\': {
                char c;
                err = parse_escape_code(p, ++ptr, &c);
                if(err) {
                    dynarr_append(p->ctx, &str, '\\');
                    dynarr_append(p->ctx, &str, *ptr);
                }
                else
                    dynarr_append(p->ctx, &str, c);
            } break;
            case '$':
                break;
            default:
                dynarr_append(p->ctx, &str, *ptr);
        }
    }

    dynarr_append(p->ctx, &str, '\0');

    *expr = (struct shard_expr) {
        .type = SHARD_EXPR_STRING,
        .loc = p->token.location,
        .string = str.items
    };

    return advance(p);
}

static int parse_with(struct parser* p, struct shard_expr* expr) {
    expr->type = SHARD_EXPR_WITH;
    expr->loc = p->token.location;
    
    expr->binop.lhs = arena_malloc(p->ctx, p->ctx->ast, sizeof(struct shard_expr));
    expr->binop.rhs = arena_malloc(p->ctx, p->ctx->ast, sizeof(struct shard_expr));

    int err[] = {
        advance(p),
        parse_expr(p, expr->binop.lhs, PREC_LOWEST),
        consume(p, SHARD_TOK_SEMICOLON),
        parse_expr(p, expr->binop.rhs, PREC_LOWEST)
    };

    return any_err(err, LEN(err));
}

static int parse_unary_op(struct parser* p, struct shard_expr* expr, enum shard_expr_type expr_type) {
    expr->type = expr_type;
    expr->loc = p->token.location; 
    expr->unaryop.expr = arena_malloc(p->ctx, p->ctx->ast, sizeof(struct shard_expr));

    int err[] = {
        advance(p),
        parse_expr(p, expr->unaryop.expr, PREC_UNARY),
    };
    
    return any_err(err, LEN(err));
}

static int parse_compound(struct parser* p, struct shard_expr* expr) {
    int err[] = {
        advance(p),
        parse_expr(p, expr, PREC_LOWEST),
        consume(p, SHARD_TOK_RPAREN)
    };

    return any_err(err, LEN(err));
}

static int parse_list(struct parser* p, struct shard_expr* expr) {
    expr->type = SHARD_EXPR_LIST;
    expr->loc = p->token.location;
    expr->list.elems = (struct shard_expr_list){0};

    int err = advance(p);
    
    struct shard_expr elem;
    while(p->token.type != SHARD_TOK_RBRACKET) {
        if(p->token.type == SHARD_TOK_EOF)
            return errorf(p, "unexpected end of file, expect expression or `]`");

        int err2 = parse_expr(p, &elem, PREC_LIST);
        if(err2)
            err = err2;
        dynarr_append(p->ctx, &expr->list.elems, elem);
    }

    int err3 = advance(p);
    return err ? err : err3;
}

static int parse_ternary(struct parser* p, struct shard_expr* expr) {
    expr->type = SHARD_EXPR_TERNARY;
    expr->loc = p->token.location;

    expr->ternary.cond = arena_malloc(p->ctx, p->ctx->ast, sizeof(struct shard_expr));
    expr->ternary.if_branch = arena_malloc(p->ctx, p->ctx->ast, sizeof(struct shard_expr));
    expr->ternary.else_branch = arena_malloc(p->ctx, p->ctx->ast, sizeof(struct shard_expr));

    int err[] = {
        advance(p),
        parse_expr(p, expr->ternary.cond, PREC_LOWEST),
        consume(p, SHARD_TOK_THEN),
        parse_expr(p, expr->ternary.if_branch, PREC_LOWEST),
        consume(p, SHARD_TOK_ELSE),
        parse_expr(p, expr->ternary.else_branch, PREC_LOWEST),
    };

    return any_err(err, LEN(err));
}

static int parse_prefix_expr(struct parser* p, struct shard_expr* expr) {
    switch(p->token.type) {
        case SHARD_TOK_IDENT:
            *expr = (struct shard_expr) {
                .type = SHARD_EXPR_IDENT,
                    .loc = p->token.location,
                    .string = p->token.value.string
            };
            return advance(p);
        case SHARD_TOK_INT:
            *expr = (struct shard_expr) {
                .type = SHARD_EXPR_INT,
                .loc = p->token.location,
                .floating = p->token.value.floating
            };
            return advance(p);
        case SHARD_TOK_FLOAT:
            *expr = (struct shard_expr) {
                .type = SHARD_EXPR_FLOAT,
                .loc = p->token.location,
                .floating = p->token.value.floating
            };
            return advance(p);
        case SHARD_TOK_NULL:
            *expr = (struct shard_expr) {
                .type = SHARD_EXPR_NULL,
                .loc = p->token.location,
            };
            return advance(p);
        case SHARD_TOK_TRUE:
            *expr = (struct shard_expr) {
                .type = SHARD_EXPR_TRUE,
                .loc = p->token.location,
            };
            return advance(p);
        case SHARD_TOK_FALSE:
            *expr = (struct shard_expr) {
                .type = SHARD_EXPR_FALSE,
                .loc = p->token.location,
            };
            return advance(p);
        case SHARD_TOK_STRING:
            return parse_string_lit(p, expr);
        case SHARD_TOK_WITH:
            return parse_with(p, expr);
        case SHARD_TOK_EXCLAMATIONMARK:
            return parse_unary_op(p, expr, SHARD_EXPR_NOT);
        case SHARD_TOK_SUB:
            return parse_unary_op(p, expr, SHARD_EXPR_NEGATE);
        case SHARD_TOK_LPAREN:
            return parse_compound(p, expr);
        case SHARD_TOK_LBRACKET:
            return parse_list(p, expr);
        case SHARD_TOK_IF:
            return parse_ternary(p, expr);
        default: {
            if(p->token.type == SHARD_TOK_ERR)
                return EINVAL;

            static char buf[1024];
            shard_dump_token(buf, sizeof(buf), &p->token);
            return errorf(p, "unexpected token `%s`, expect expression", buf);
        }
    }

    return 0;
}

static int parse_addition(struct parser* p, struct shard_expr* expr, struct shard_expr* left) {
    expr->type = p->token.type == SHARD_TOK_ADD ? SHARD_EXPR_ADD : SHARD_EXPR_SUB;
    expr->binop.lhs = arena_malloc(p->ctx, p->ctx->ast, sizeof(struct shard_expr));
    expr->binop.rhs = arena_malloc(p->ctx, p->ctx->ast, sizeof(struct shard_expr));
    
    *expr->binop.lhs = *left;

    int err[] = {
        advance(p),
        parse_expr(p, expr->binop.rhs, PREC_ADDITION)
    };

    return any_err(err, LEN(err));
}

static int parse_call(struct parser* p, struct shard_expr* expr, struct shard_expr* left) {
    expr->type = SHARD_EXPR_CALL;
    expr->binop.lhs = arena_malloc(p->ctx, p->ctx->ast, sizeof(struct shard_expr));
    expr->binop.rhs = arena_malloc(p->ctx, p->ctx->ast, sizeof(struct shard_expr));

    *expr->binop.lhs = *left;

    return parse_expr(p, expr->binop.rhs, PREC_CALL);
}

static inline bool token_terminates_expr(enum shard_token_type type) {
    switch(type) {
        case SHARD_TOK_RPAREN:
        case SHARD_TOK_RBRACE:
        case SHARD_TOK_RBRACKET:
        case SHARD_TOK_SEMICOLON:
        case SHARD_TOK_COLON:
        case SHARD_TOK_EOF:
        case SHARD_TOK_ELSE:
        case SHARD_TOK_THEN:
            return true;
        default:
            return false;
    }
}

static enum precedence get_precedence(enum shard_token_type type) {
    switch(type) {
        case SHARD_TOK_ADD:
        case SHARD_TOK_SUB:
            return PREC_ADDITION;
        default:
            return token_terminates_expr(type) ? PREC_LOWEST : PREC_CALL;
    }
}

static int parse_infix_expr(struct parser* p, struct shard_expr* expr, struct shard_expr* left) {
    switch(p->token.type) {
        case SHARD_TOK_ADD:
        case SHARD_TOK_SUB:
            return parse_addition(p, expr, left);
        default:
            if(!token_terminates_expr(p->token.type))
                return parse_call(p, expr, left);
            else {
                if(p->token.type == SHARD_TOK_ERR)
                    return EINVAL;

                static char buf[1024];
                shard_dump_token(buf, sizeof(buf), &p->token);
                return errorf(p, "unexpected token `%s`, expect infix expression", buf);
            }
    }
}

static int parse_expr(struct parser* p, struct shard_expr* expr, enum precedence prec) {
    struct shard_expr tmp;
    int err = parse_prefix_expr(p, expr);

    while(prec < get_precedence(p->token.type)) {
        int err = parse_infix_expr(p, &tmp, expr);
        if(err)
            return err;
        *expr = tmp;
    }

    return err;
}

int shard_parse(struct shard_context* ctx, struct shard_source* src, struct shard_expr* expr) {
    struct parser p = {
        .ctx = ctx,
        .src = src,
        .token = {0}
    };

    int err[] = {
        advance(&p),
        parse_expr(&p, expr, PREC_LOWEST)
    };

    return any_err(err, LEN(err));
}

