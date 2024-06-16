#define _LIBSHARD_INTERNAL
#include <libshard.h>

#include <stdio.h>
#include <stdarg.h>
#include <setjmp.h>
#include <errno.h>

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

#define LIST_VAL(head) ((struct shard_value) { .type = SHARD_VAL_LIST, .list.head = (head) })

struct scope {
    struct scope* outer;
    struct shard_hashmap* idents;
};

struct evaluator {
    struct shard_context* ctx;
    struct shard_gc* gc;

    struct scope* scope;
    jmp_buf* exception;
};

static void evaluator_init(volatile struct evaluator* eval, struct shard_context* ctx, jmp_buf* exception) {
    memset((void*) eval, 0, sizeof(struct evaluator));
    eval->ctx = ctx;
    eval->exception = exception;
    eval->gc = &ctx->gc;
}

static void evaluator_free(volatile struct evaluator* eval) {
    (void) eval;
}

static inline void scope_push(volatile struct evaluator* e, struct scope* scope) {
    scope->outer = e->scope;
    e->scope = scope;
}

static inline struct scope* scope_pop(volatile struct evaluator* e) {
    if(!e->scope)
        return NULL;
    
    struct scope* scope = e->scope;
    e->scope = scope->outer;
    return scope;
}

static inline void* lookup_var(volatile struct evaluator* e, shard_ident_t ident) {
    struct scope* scope = e->scope;
    void* found = NULL;
    while(scope) {
        found = shard_hashmap_get(scope->idents, ident);
        if(found)
            return found;
        scope = scope->outer;
    }

    return NULL;
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

static inline struct shard_value eval_list(volatile struct evaluator* e, struct shard_expr* expr) {
    struct shard_list *head = NULL, *current = NULL, *next;
    
    for(size_t i = 0; i < expr->list.elems.count; i++) {
        next = shard_gc_malloc(e->gc, sizeof(struct shard_list));
        next->next = NULL;
        next->value.lazy = &expr->list.elems.items[i];
        next->value.evaluated = false;

        if(current)
            current->next = next;
        current = next;
        if(!head)
            head = current;
    }

    return LIST_VAL(head);
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
                    sum = shard_gc_malloc(e->gc, (len + 1) * sizeof(char));
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

#define ARITH_OP_INT_FLT(op) \
    struct shard_value left = eval(e, expr->binop.lhs), \
                      right = eval(e, expr->binop.rhs); \
 \
    bool left_err = false; \
    switch(left.type) { \
        case SHARD_VAL_INT: { \
            int64_t sum = left.integer; \
            switch(right.type) { \
                case SHARD_VAL_INT: \
                    sum = sum op right.integer; \
                    break; \
                case SHARD_VAL_FLOAT: \
                    sum = sum op (double) right.floating; \
                    break; \
                default: \
                    goto fail; \
            } \
            return INT_VAL(sum); \
        } break; \
        case SHARD_VAL_FLOAT: { \
            double sum = left.floating; \
            switch(right.type) { \
                case SHARD_VAL_INT: \
                    sum = sum op (double) right.integer; \
                    break; \
                case SHARD_VAL_FLOAT: \
                    sum = sum op right.floating; \
                    break; \
                default: \
                    goto fail; \
            } \
            return FLOAT_VAL(sum); \
        } break; \
        default: \
            left_err = true; \
            break; \
    } \
 \
fail: \
    throw(e, expr->loc, "`" #op "`: %s operand is not of a numeric type", left_err ? "left" : "right"); \

static inline struct shard_value eval_subtraction(volatile struct evaluator* e, struct shard_expr* expr) {
    ARITH_OP_INT_FLT(-)
}

static inline struct shard_value eval_multiplication(volatile struct evaluator* e, struct shard_expr* expr) {
    ARITH_OP_INT_FLT(*)
}

static inline struct shard_value eval_division(volatile struct evaluator* e, struct shard_expr* expr) {
    ARITH_OP_INT_FLT(/) // TODO: handle zero-division
}

static inline struct shard_value eval_negation(volatile struct evaluator* e, struct shard_expr* expr) {
    struct shard_value value = eval(e, expr->unaryop.expr);
    switch(value.type) {
        case SHARD_VAL_INT:
            return INT_VAL(-value.integer);
        case SHARD_VAL_FLOAT:
            return FLOAT_VAL(-value.floating);
        default:
            throw(e, expr->loc, "`-`: operand is not of a numeric type");
    }
}

static inline bool is_truthy(struct shard_value value) {
    switch(value.type) {
        case SHARD_VAL_NULL:
            return false;
        case SHARD_VAL_BOOL:
            return value.boolean;
        case SHARD_VAL_INT:
            return value.integer != 0;
        case SHARD_VAL_FLOAT:
            return value.floating != .0f;
        case SHARD_VAL_STRING:
            return value.strlen != 0;
        case SHARD_VAL_PATH:
            return value.pathlen != 0;
        case SHARD_VAL_LIST:
            return value.list.head != NULL;
        case SHARD_VAL_SET:
            return value.set.set_size != 0;
        case SHARD_VAL_FUNCTION:
            return true;
        default:
            return false;
    }
}

static inline struct shard_value eval_logand(volatile struct evaluator* e, struct shard_expr* expr) {
    return BOOL_VAL(is_truthy(eval(e, expr->binop.lhs)) && is_truthy(eval(e, expr->binop.rhs)));
}

static inline struct shard_value eval_logor(volatile struct evaluator* e, struct shard_expr* expr) {
    return BOOL_VAL(is_truthy(eval(e, expr->binop.lhs)) || is_truthy(eval(e, expr->binop.rhs)));
}

static inline struct shard_value eval_logimpl(volatile struct evaluator* e, struct shard_expr* expr) {
    return BOOL_VAL(!is_truthy(eval(e, expr->binop.lhs)) || is_truthy(eval(e, expr->binop.rhs)));
}

static inline struct shard_value eval_not(volatile struct evaluator* e, struct shard_expr* expr) {
    return BOOL_VAL(!is_truthy(eval(e, expr->unaryop.expr)));
}

static inline struct shard_value eval_ternary(volatile struct evaluator* e, struct shard_expr* expr) {
    return is_truthy(eval(e, expr->ternary.cond)) ? eval(e, expr->ternary.if_branch) : eval(e, expr->ternary.else_branch);
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
        case SHARD_EXPR_LIST:
            return eval_list(e, expr);
        case SHARD_EXPR_EQ:
            return eval_eq(e, expr, false);
        case SHARD_EXPR_NE:
            return eval_eq(e, expr, true);
        case SHARD_EXPR_ADD:
            return eval_addition(e, expr);
        case SHARD_EXPR_SUB:
            return eval_subtraction(e, expr);
        case SHARD_EXPR_MUL:
            return eval_multiplication(e, expr);
        case SHARD_EXPR_DIV:
            return eval_division(e, expr);
        case SHARD_EXPR_LOGAND:
            return eval_logand(e, expr);
        case SHARD_EXPR_LOGOR:
            return eval_logor(e, expr);
        case SHARD_EXPR_LOGIMPL:
            return eval_logimpl(e, expr);
        case SHARD_EXPR_NOT:
            return eval_not(e, expr);
        case SHARD_EXPR_NEGATE:
            return eval_negation(e, expr);
        case SHARD_EXPR_TERNARY:
            return eval_ternary(e, expr);
        default:
            throw(e, expr->loc, "unimplemented expression `%d`.", expr->type);
    }
}

int shard_eval_lazy(struct shard_context* ctx, struct shard_lazy_value* value) {
    if(value->evaluated)
        return 0;

    jmp_buf exception;
    volatile struct evaluator e;
    evaluator_init(&e, ctx, &exception);

    if(setjmp(*e.exception) == 0) {
        value->eval = eval(&e, value->lazy);
        value->evaluated = true;
    }

    evaluator_free(&e);

    return ctx->errors.count;
}

int shard_eval(struct shard_context* ctx, struct shard_source* src, struct shard_value* result, struct shard_expr* expr) {
    int err = shard_parse(ctx, src, expr);
    if(err)
        goto finish;

    jmp_buf exception;
    volatile struct evaluator e;
    evaluator_init(&e, ctx, &exception);

    if(setjmp(*e.exception) == 0) {
        *result = eval(&e, expr);
    }

    evaluator_free(&e);
finish:
    return ctx->errors.count;
}


