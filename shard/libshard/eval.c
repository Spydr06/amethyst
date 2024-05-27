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
#define STRING_VAL(s) ((struct shard_value) { .type = SHARD_VAL_STRING, .string = (s) })

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
        .path = path.items
    };
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
            return STRING_VAL(expr->string);
        case SHARD_EXPR_PATH:
            return eval_path(e, expr);
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


