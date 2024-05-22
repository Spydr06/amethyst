#define _LIBSHARD_INTERNAL
#include <libshard.h>

#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

enum precedence {
    PREC_FUNCTION = 1,
    PREC_ADDITION,
    PREC_MULTIPLICATION,
    PREC_UNARY,
};

static int error(struct shard_context* ctx, struct shard_location loc, char* msg) {
    dynarr_append(ctx, &ctx->errors, ((struct shard_error){
        .err = msg,
        .heap = false,
        .loc = loc,
        ._errno = EINVAL
    }));

    return EINVAL;
}

static int errorf(struct shard_context* ctx, struct shard_location loc, const char* fmt, ...) {
    char* buffer = ctx->malloc(256);
    
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buffer, 1024, fmt, ap);
    va_end(ap);

    dynarr_append(ctx, &ctx->errors, ((struct shard_error){
        .err = buffer,
        .heap = true,
        .loc = loc,
        ._errno = EINVAL
    }));

    return EINVAL;
}

static int next_token(struct shard_context* ctx, struct shard_source* src, struct shard_token* token) {
    int err = shard_lex(ctx, src, token);
    if(err) {
        dynarr_append(ctx, &ctx->errors, ((struct shard_error){
            .err = token->value.string,
            .loc = token->location,
            ._errno = err
        }));
    }

    return err;
}

static int consume(struct shard_context* ctx, struct shard_source* src, enum shard_token_type token_type) {
    struct shard_token token;
    int err = next_token(ctx, src, &token);
    if(err)
        return err;

    if(token.type == token_type)
        return 0;

    return errorf(
        ctx, token.location,
        "unexpected token `%s`, expect token `%s`",
        shard_token_type_to_str(token.type),
        shard_token_type_to_str(token_type)
    );
}

static int parse_escape_code(struct shard_context* ctx, struct shard_location loc, const char* ptr, char* c) {
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
            return errorf(ctx, loc, "invalid escape code: `\\%c`", *ptr);
    }
    return 0;
}

static int parse_string_lit(struct shard_context* ctx, struct shard_token* token, struct shard_expr* expr) {
    struct shard_string str = {0};

    int err = 0;
    for(const char* ptr = token->value.string; *ptr; ptr++) {
        switch(*ptr) {
            case '\\': {
                char c;
                err = parse_escape_code(ctx, token->location, ++ptr, &c);
                if(err) {
                    dynarr_append(ctx, &str, '\\');
                    dynarr_append(ctx, &str, *ptr);
                }
                else
                    dynarr_append(ctx, &str, c);
            } break;
            case '$':
                break;
            default:
                dynarr_append(ctx, &str, *ptr);
        }
    }

    dynarr_append(ctx, &str, '\0');

    *expr = (struct shard_expr) {
        .type = SHARD_EXPR_STRING,
        .loc = token->location,
        .string = str.items
    };

    return 0;
}

static int parse_with(struct shard_context* ctx, struct shard_source* src, struct shard_token* token, struct shard_expr* expr) {
    expr->type = SHARD_EXPR_WITH;
    expr->loc = token->location;
    
    expr->binop.lhs = arena_malloc(ctx, ctx->ast, sizeof(struct shard_expr));
    expr->binop.rhs = arena_malloc(ctx, ctx->ast, sizeof(struct shard_expr));

    int err[] = {
        shard_parse(ctx, src, expr->binop.lhs),
        consume(ctx, src, SHARD_TOK_SEMICOLON),
        shard_parse(ctx, src, expr->binop.rhs)
    };

    return EITHER(err[0], EITHER(err[1], err[2]));
}

static int parse_unary_op(struct shard_context* ctx, struct shard_source* src, struct shard_token* token, struct shard_expr* expr, enum shard_expr_type expr_type) {
    expr->type = expr_type;
    expr->loc = token->location;
    
    expr->unaryop.expr = arena_malloc(ctx, ctx->ast, sizeof(struct shard_expr));
    return shard_parse(ctx, src, expr->unaryop.expr);
}

static int parse_compound(struct shard_context* ctx, struct shard_source* src, struct shard_expr* expr) {
    int err[] = {
        shard_parse(ctx, src, expr),
        consume(ctx, src, SHARD_TOK_RPAREN)
    };

    return EITHER(err[0], err[1]);
}

static int parse_ternary(struct shard_context* ctx, struct shard_source* src, struct shard_token* token, struct shard_expr* expr) {
    expr->type = SHARD_EXPR_TERNARY;
    expr->loc = token->location;

    expr->ternary.cond = arena_malloc(ctx, ctx->ast, sizeof(struct shard_expr));
    expr->ternary.if_branch = arena_malloc(ctx, ctx->ast, sizeof(struct shard_expr));
    expr->ternary.else_branch = arena_malloc(ctx, ctx->ast, sizeof(struct shard_expr));

    int err[] = {
        shard_parse(ctx, src, expr->ternary.cond),
        consume(ctx, src, SHARD_TOK_THEN),
        shard_parse(ctx, src, expr->ternary.if_branch),
        consume(ctx, src, SHARD_TOK_ELSE),
        shard_parse(ctx, src, expr->ternary.else_branch),
    };

    return EITHER(err[0], EITHER(err[1], EITHER(err[2], EITHER(err[3], err[4]))));
}

int shard_parse(struct shard_context* ctx, struct shard_source* src, struct shard_expr* expr) {
    struct shard_token token;
    int err = next_token(ctx, src, &token);
    if(err)
        return errorf(ctx, token.location, "lexer error: %s", token.value.string);

    switch(token.type) {
        case SHARD_TOK_NUMBER:
            *expr = (struct shard_expr) {
                .type = SHARD_EXPR_NUMBER,
                .loc = token.location,
                .number = token.value.number
            };
            break;
        case SHARD_TOK_TRUE:
            *expr = (struct shard_expr) {
                .type = SHARD_EXPR_TRUE,
                .loc = token.location,
            };
            break;
        case SHARD_TOK_FALSE:
            *expr = (struct shard_expr) {
                .type = SHARD_EXPR_FALSE,
                .loc = token.location,
            };
            break;
        case SHARD_TOK_STRING:
            return parse_string_lit(ctx, &token, expr);
        case SHARD_TOK_WITH:
            return parse_with(ctx, src, &token, expr);
        case SHARD_TOK_EXCLAMATIONMARK:
            return parse_unary_op(ctx, src, &token, expr, SHARD_EXPR_NOT);
        case SHARD_TOK_SUB:
            return parse_unary_op(ctx, src, &token, expr, SHARD_EXPR_NEGATE);
        case SHARD_TOK_LPAREN:
            return parse_compound(ctx, src, expr);
        case SHARD_TOK_IF:
            return parse_ternary(ctx, src, &token, expr);
        default: {
            static char buf[1024];
            shard_dump_token(buf, sizeof(buf), &token);
            return errorf(ctx, token.location, "unexpected token `%s`, expect expression", buf);
        }
    }

    return 0;
}

