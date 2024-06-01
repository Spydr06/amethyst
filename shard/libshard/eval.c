#define _LIBSHARD_INTERNAL
#include <libshard.h>

#include <stdio.h>
#include <stdarg.h>
#include <setjmp.h>
#include <errno.h>
#include <limits.h>

#ifndef PATH_MAX
    #define PATH_MAX 4096
#endif

#define NULL_VAL() ((struct shard_value) { .type = SHARD_VAL_NULL })
#define TRUE_VAL() BOOL_VAL(true)
#define FALSE_VAL() BOOL_VAL(false)

#define BOOL_VAL(b) ((struct shard_value) { .type = SHARD_VAL_BOOL, .boolean = (bool)(b) })
#define INT_VAL(i) ((struct shard_value) { .type = SHARD_VAL_INT, .integer = (int64_t)(i) })
#define FLOAT_VAL(f) ((struct shard_value) { .type = SHARD_VAL_FLOAT, .floating = (double)(f) })
#define STRING_VAL(s, l) ((struct shard_value) { .type = SHARD_VAL_STRING, .string = (s), .strlen = (l) })

struct scope {
    struct scope* outer;
};

struct evaluator {
    struct shard_context* ctx;
    struct shard_gc gc;

    struct scope* scope;
    jmp_buf* exception;
};

static void evaluator_init(volatile struct evaluator* eval, struct shard_context* ctx, jmp_buf* exception, void* stack_bottom) {
    memset((void*) eval, 0, sizeof(struct evaluator));
    eval->ctx = ctx;
    eval->exception = exception;

    shard_gc_begin(&eval->gc, ctx, stack_bottom);
}

static void evaluator_free(volatile struct evaluator* eval) {
    shard_gc_end(&eval->gc);
}

#define assert_type(e, loc, a, b, ...)  do {    \
        if(!((a) & (b)))                        \
            throw((e), (loc), __VA_ARGS__);     \
    while(0)

static _Noreturn __attribute__((format(printf, 3, 4))) 
void throw(volatile struct evaluator* e, struct shard_location loc, const char* fmt, ...) {
    char* buffer = e->ctx->malloc(1024);
    
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buffer, 1024, fmt, ap);
    va_end(ap);

    dynarr_append(e->ctx, &e->ctx->errors, ((struct shard_error){
        .err = buffer,
        .heap = true,
        .loc = loc,
        ._errno = EINVAL
    }));

    longjmp(*e->exception, 1);
}

static inline struct shard_value eval_path(volatile struct evaluator* e, struct shard_expr* expr) {
    static char tmpbuf[PATH_MAX + 1];

    struct shard_string path = {0};
    // TODO: buffer resolved paths
    switch(*expr->string) {
        case '/':
            shard_string_append(e->ctx, &path, expr->string);
            break;
        case '.': { // TODO: do with lesser allocations
            if(!e->ctx->realpath || !e->ctx->dirname || !expr->loc.src->origin)
                throw(e, expr->loc, "relative paths are disabled");

            char* current_path = e->ctx->malloc(strlen(expr->loc.src->origin) + 1);
            strcpy(current_path, expr->loc.src->origin);
            e->ctx->dirname(current_path);

            if(!e->ctx->realpath(current_path, tmpbuf))
                throw(e, expr->loc, "could not resolve relative path");

            e->ctx->free(current_path);
            
            shard_string_append(e->ctx, &path, tmpbuf);
            if(path.items[path.count - 1] != '/')
                shard_string_push(e->ctx, &path, '/');
            
            shard_string_append(e->ctx, &path, expr->string + 2);
        } break;
        case '~':
            if(!e->ctx->home_dir)
                throw(e, expr->loc, "no home directory specified; Home paths are disabled");

            shard_string_append(e->ctx, &path, e->ctx->home_dir);
            if(path.items[path.count - 1] != '/')
                shard_string_push(e->ctx, &path, '/');
            
            shard_string_append(e->ctx, &path, expr->string + 2);
            break;
        default:
            // include paths...
            shard_string_append(e->ctx, &path, expr->string);
            break;
    }

    // TODO: register `path` with garbage collector
    return (struct shard_value) {
        .type = SHARD_VAL_PATH,
        .path = path.items,
        .pathlen = path.count
    };
}

static struct shard_value eval(volatile struct evaluator* e, struct shard_expr* expr);

static inline struct shard_value eval_eq(volatile struct evaluator* e, struct shard_expr* expr, bool negate) {
    struct shard_value values[] = {
        eval(e, expr->binop.lhs),
        eval(e, expr->binop.rhs)
    };

    if(values[0].type != values[1].type)
        return BOOL_VAL(false ^ negate);

    switch(values[0].type) {
        case SHARD_VAL_NULL:
            return BOOL_VAL(true ^ negate);
        case SHARD_VAL_INT:
            return BOOL_VAL((values[0].integer == values[1].integer) ^ negate);
        case SHARD_VAL_FLOAT:
            return BOOL_VAL((values[0].floating == values[1].floating) ^ negate);
        case SHARD_VAL_BOOL:
            return BOOL_VAL((values[0].boolean == values[1].boolean) ^ negate);
        case SHARD_VAL_PATH:
            return BOOL_VAL((strcmp(values[0].path, values[1].path) == 0) ^ negate);
        case SHARD_VAL_STRING:
            return BOOL_VAL((strcmp(values[0].string, values[1].string) == 0) ^ negate);
        case SHARD_VAL_FUNCTION:
            return BOOL_VAL((memcmp(&values[0].function, &values[1].function, sizeof(values[0].function)) == 0) ^ negate);
        case SHARD_VAL_SET:
        case SHARD_VAL_LIST:
            throw(e, expr->loc, "unimplemented");
    }
}

static inline struct shard_value eval_addition(volatile struct evaluator* e, struct shard_expr* expr) {
    struct shard_value values[] = {
        eval(e, expr->binop.lhs),
        eval(e, expr->binop.rhs)
    };

    bool left = false;
    switch(values[0].type) {
        case SHARD_VAL_INT: {
            int64_t sum = values[0].integer;
            switch(values[1].type) {
                case SHARD_VAL_INT:
                    sum += values[1].integer;
                    break;
                case SHARD_VAL_FLOAT:
                    sum += (int64_t) values[1].floating;
                    break;
                default:
                    goto fail;
            }

            return INT_VAL(sum);
        }

        case SHARD_VAL_FLOAT: {
            double sum = values[0].floating;
            switch(values[1].type) {
                case SHARD_VAL_INT:
                    sum += (double) values[1].integer;
                    break;
                case SHARD_VAL_FLOAT:
                    sum += values[1].floating;
                    break;
                default:
                    goto fail;
            }

            return FLOAT_VAL(sum);
        }
        
        case SHARD_VAL_STRING: {
            char* sum = 0;
            size_t len = 0;
            switch(values[1].type) {
                case SHARD_VAL_STRING:
                    len = values[0].strlen + values[1].strlen;
                    sum = shard_gc_malloc(&e->gc, (len + 1) * sizeof(char));
                    sum[0] = '\0';
                    strcat(sum, values[0].string);
                    strcat(sum, values[1].string);
                    break;
                default:
                    goto fail;
            }
            return STRING_VAL(sum, len);
        }
        
        default:
            left = true;
            break;
    }

fail:
    throw(e, expr->loc, "`+`: %s operand is not of a numeric type", left ? "left" : "right");
}

static struct shard_value eval_subtraction(volatile struct evaluator* e, struct shard_expr* expr) {
    struct shard_value left = eval(e, expr->binop.lhs),
                      right = eval(e, expr->binop.rhs);

    bool left_err = false;
    switch(left.type) {
        case SHARD_VAL_INT: {
            int64_t sum = left.integer;
            switch(right.type) {
                case SHARD_VAL_INT:
                    sum -= right.integer;
                    break;
                case SHARD_VAL_FLOAT:
                    sum -= (double) right.floating;
                    break;
                default:
                    goto fail;
            }
            return INT_VAL(sum);
        } break;
        case SHARD_VAL_FLOAT: {
            double sum = left.floating;
            switch(right.type) {
                case SHARD_VAL_INT:
                    sum -= (double) right.integer;
                    break;
                case SHARD_VAL_FLOAT:
                    sum -= right.floating;
                    break;
                default:
                    goto fail;
            }
            return FLOAT_VAL(sum);
        } break;
        default:
            left_err = true;
            break;
    }

fail:
    throw(e, expr->loc, "`-`: %s operand is not of a numeric type", left_err ? "left" : "right");
}

static struct shard_value eval(volatile struct evaluator* e, struct shard_expr* expr) {
    switch(expr->type) {
        case SHARD_EXPR_INT:
            return INT_VAL(expr->integer);
        case SHARD_EXPR_FLOAT:
            return FLOAT_VAL(expr->floating);
        case SHARD_EXPR_NULL:
            return NULL_VAL();
        case SHARD_EXPR_TRUE:
            return TRUE_VAL();
        case SHARD_EXPR_FALSE:
            return FALSE_VAL();
        case SHARD_EXPR_STRING:
            return STRING_VAL(expr->string, strlen(expr->string));
        case SHARD_EXPR_PATH:
            return eval_path(e, expr);
        case SHARD_EXPR_EQ:
            return eval_eq(e, expr, false);
        case SHARD_EXPR_NE:
            return eval_eq(e, expr, true);
        case SHARD_EXPR_ADD:
            return eval_addition(e, expr);
        case SHARD_EXPR_SUB:
            return eval_subtraction(e, expr);
        default:
            throw(e, expr->loc, "unimplemented expression `%d`.", expr->type);
    }
}

int shard_eval(struct shard_context* ctx, struct shard_source* src, struct shard_value* result) {
    struct shard_expr expr = {0};
    int err = shard_parse(ctx, src, &expr);
    if(err)
        goto finish;

    jmp_buf exception;
    volatile struct evaluator e;
    evaluator_init(&e, ctx, &exception, __builtin_frame_address(0));

    if(setjmp(*e.exception) == 0)
        *result = eval(&e, &expr);

    evaluator_free(&e);
finish:
    shard_free_expr(ctx, &expr);
    return ctx->errors.count;
}


