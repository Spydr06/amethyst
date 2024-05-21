#define _LIBSHARD_INTERNAL
#include <libshard.h>

#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

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

    *expr = (struct shard_expr) {
        .type = SHARD_EXPR_STRING,
        .loc = token->location,
        .string = str.items
    };

    return 0;
}

static int parse_with(struct shard_context* ctx, struct shard_source* src, struct shard_expr* expr) {

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
            return parse_with(ctx, &token, expr);
        default: {
            static char buf[1024];
            shard_dump_token(buf, sizeof(buf), &token);
            return errorf(ctx, token.location, "unexpected token `%s`, expect expression", buf);
        }
    }

    return 0;
}

