#define _LIBSHARD_INTERNAL
#include <libshard.h>

#include <stdio.h>
#include <stdarg.h>
#include <setjmp.h>
#include <errno.h>

#ifndef PATH_MAX
    #define PATH_MAX 4096
#endif

static struct shard_value eval(volatile struct shard_evaluator* e, struct shard_expr* expr);

static void evaluator_init(volatile struct shard_evaluator* eval, struct shard_context* ctx, jmp_buf* exception) {
    memset((void*) eval, 0, sizeof(struct shard_evaluator));
    eval->ctx = ctx;
    eval->exception = exception;
    eval->gc = &ctx->gc;
}

static void evaluator_free(volatile struct shard_evaluator* eval) {
    (void) eval;
}

static inline struct shard_scope* scope_init(volatile struct shard_evaluator* e, struct shard_set* bindings) {
    struct shard_scope* scope = shard_gc_malloc(e->gc, sizeof(struct shard_scope));
    scope->bindings = bindings;
    scope->outer = e->scope;
    return scope;
}

static inline void scope_push(volatile struct shard_evaluator* e, struct shard_set* bindings) {
    e->scope = scope_init(e, bindings);
}

static inline struct shard_scope* scope_pop(volatile struct shard_evaluator* e) {
    if(!e->scope)
        return NULL;
    
    struct shard_scope* scope = e->scope;
    e->scope = scope->outer;
    return scope;
}

static inline struct shard_lazy_value* lookup_binding(volatile struct shard_evaluator* e, shard_ident_t ident) {
    struct shard_scope* scope = e->scope;
    struct shard_lazy_value* found = NULL;
    while(scope) {
        int err = shard_set_get(scope->bindings, ident, &found);
        if(!err && found)
            return found;
        scope = scope->outer;
    }

    return NULL;
}

#define assert_type(e, loc, a, b, ...)  do {    \
        if(!((a) & (b)))                        \
            shard_eval_throw((e), (loc), __VA_ARGS__);     \
    while(0)

_Noreturn __attribute__((format(printf, 3, 4))) 
void shard_eval_throw(volatile struct shard_evaluator* e, struct shard_location loc, const char* fmt, ...) {
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

    longjmp(*e->exception, SHARD_EVAL_ERROR);
}

static inline struct shard_value eval_lazy(volatile struct shard_evaluator* e, struct shard_lazy_value* lazy) {
    if(!lazy->evaluated) {
        struct shard_scope* prev = e->scope;
        e->scope = lazy->scope;
        lazy->eval = eval(e, lazy->lazy);
        lazy->evaluated = true;
        e->scope = prev;
    }
    return lazy->eval;
}

struct shard_value shard_eval_lazy2(volatile struct shard_evaluator* e, struct shard_lazy_value* value) {
    return eval_lazy(e, value);
}

static inline struct shard_value eval_path(volatile struct shard_evaluator* e, struct shard_expr* expr) {
    static char tmpbuf[PATH_MAX + 1];

    struct shard_string path = {0};
    // TODO: buffer resolved paths
    switch(*expr->string) {
        case '/':
            shard_gc_string_append(e->gc, &path, expr->string);
            break;
        case '.': { // TODO: do with lesser allocations
            if(!e->ctx->realpath || !e->ctx->dirname || !expr->loc.src->origin)
                shard_eval_throw(e, expr->loc, "relative paths are disabled");

            char* current_path = e->ctx->malloc(strlen(expr->loc.src->origin) + 1);
            strcpy(current_path, expr->loc.src->origin);
            e->ctx->dirname(current_path);

            if(!e->ctx->realpath(current_path, tmpbuf))
                shard_eval_throw(e, expr->loc, "could not resolve relative path");

            e->ctx->free(current_path);
            
            shard_gc_string_append(e->gc, &path, tmpbuf);
            if(path.items[path.count - 1] != '/')
                shard_gc_string_push(e->gc, &path, '/');
            
            shard_gc_string_append(e->gc, &path, expr->string + 2);
        } break;
        case '~':
            if(!e->ctx->home_dir)
                shard_eval_throw(e, expr->loc, "no home directory specified; Home paths are disabled");

            shard_gc_string_append(e->gc, &path, e->ctx->home_dir);
            if(path.items[path.count - 1] != '/')
                shard_gc_string_push(e->gc, &path, '/');
            
            shard_gc_string_append(e->gc, &path, expr->string + 2);
            break;
        default:
            // include paths...
            shard_gc_string_append(e->gc, &path, expr->string);
            break;
    }

    shard_gc_string_push(e->gc, &path, '\0');

    return PATH_VAL(path.items, path.count);
}

static inline struct shard_value eval_list(volatile struct shard_evaluator* e, struct shard_expr* expr) {
    struct shard_list *head = NULL, *current = NULL, *next;
    
    for(size_t i = 0; i < expr->list.elems.count; i++) {
        next = shard_gc_malloc(e->gc, sizeof(struct shard_list));
        next->next = NULL;
        next->value = shard_lazy(e->ctx, &expr->list.elems.items[i], e->scope);

        if(current)
            current->next = next;
        current = next;
        if(!head)
            head = current;
    }

    return LIST_VAL(head);
}

static inline struct shard_value eval_set(volatile struct shard_evaluator* e, struct shard_expr* expr) {
    struct shard_set* set = shard_set_from_hashmap(e, &expr->set.attrs);
    if(!set)
        shard_eval_throw(e, expr->loc, "error allocating set: %s", strerror(errno));
    
    if(expr->set.recursive) {
        struct shard_scope* scope = scope_init(e, set);
        for(size_t i = 0; i < set->capacity; i++) {
            struct shard_lazy_value* value = set->entries[i].value;
            if(value->evaluated)
                continue;

            value->scope = scope;
        }
    }

    return SET_VAL(set);
}

static inline struct shard_value eval_function(volatile struct shard_evaluator* e, struct shard_expr* expr) {
    return FUNC_VAL(expr->func.arg, expr->func.body, e->scope);
}

static struct shard_value eval(volatile struct shard_evaluator* e, struct shard_expr* expr);

static inline struct shard_value eval_eq(volatile struct shard_evaluator* e, struct shard_expr* expr, bool negate) {
    struct shard_value values[] = {
        eval(e, expr->binop.lhs),
        eval(e, expr->binop.rhs)
    };

    return BOOL_VAL(shard_values_equal(&values[0], &values[1]) ^ negate);
}

#define CMP_OP(e, expr, op) do {            \
        bool left_err = true;               \
        struct shard_value values[] = {     \
            eval((e), (expr)->binop.lhs),   \
            eval((e), (expr)->binop.rhs)    \
        };                                  \
                                            \
        switch(values[0].type) {            \
            case SHARD_VAL_INT:             \
                switch(values[1].type) {    \
                    case SHARD_VAL_INT:     \
                        return BOOL_VAL(values[0].integer op values[1].integer);   \
                    case SHARD_VAL_FLOAT:   \
                        return BOOL_VAL(values[0].integer op (int64_t) values[1].floating); \
                    default:                \
                        left_err = false;   \
                        goto error;         \
                }                           \
            case SHARD_VAL_FLOAT:           \
                switch(values[1].type) {    \
                    case SHARD_VAL_INT:     \
                        return BOOL_VAL(values[0].floating op (double) values[1].integer);   \
                    case SHARD_VAL_FLOAT:   \
                        return BOOL_VAL(values[0].floating op values[1].floating); \
                    default:                \
                        left_err = false;   \
                        goto error;         \
                }                           \
            default:                        \
                goto error;                 \
        }                                   \
    error:                                  \
        shard_eval_throw(e, expr->loc, "`" #op "`: %s operand is not of a numeric type", left_err ? "left" : "right");\
    } while(0)

static inline struct shard_value eval_le(volatile struct shard_evaluator* e, struct shard_expr* expr) {
    CMP_OP(e, expr, <=);
}

static inline struct shard_value eval_lt(volatile struct shard_evaluator* e, struct shard_expr* expr) {
    CMP_OP(e, expr, <);
}

static inline struct shard_value eval_ge(volatile struct shard_evaluator* e, struct shard_expr* expr) {
    CMP_OP(e, expr, >=);
}

static inline struct shard_value eval_gt(volatile struct shard_evaluator* e, struct shard_expr* expr) {
    CMP_OP(e, expr, >);
}

static inline struct shard_value eval_addition(volatile struct shard_evaluator* e, struct shard_expr* expr) {
    return shard_eval_addition(
        e,
        eval(e, expr->binop.lhs),
        eval(e, expr->binop.rhs),
        &expr->loc
    );
}

struct shard_value shard_eval_addition(volatile struct shard_evaluator* e, struct shard_value left, struct shard_value right, struct shard_location* loc) {
    bool is_left = false;
    switch(left.type) {
        case SHARD_VAL_INT: {
            int64_t sum = left.integer;
            switch(right.type) {
                case SHARD_VAL_INT:
                    sum += right.integer;
                    break;
                case SHARD_VAL_FLOAT:
                    sum += (int64_t) right.floating;
                    break;
                default:
                    goto fail;
            }

            return INT_VAL(sum);
        }

        case SHARD_VAL_FLOAT: {
            double sum = left.floating;
            switch(right.type) {
                case SHARD_VAL_INT:
                    sum += (double) right.integer;
                    break;
                case SHARD_VAL_FLOAT:
                    sum += right.floating;
                    break;
                default:
                    goto fail;
            }

            return FLOAT_VAL(sum);
        }
        
        case SHARD_VAL_STRING: {
            char* sum = 0;
            size_t len = 0;
            switch(right.type) {
                case SHARD_VAL_STRING:
                    len = left.strlen + right.strlen;
                    sum = shard_gc_malloc(e->gc, (len + 1) * sizeof(char));
                    sum[0] = '\0';
                    strcat(sum, left.string);
                    strcat(sum, right.string);
                    break;
                default:
                    goto fail;
            }
            return STRING_VAL(sum, len);
        }

        case SHARD_VAL_PATH: {
            char* sum = 0;
            size_t len = 0;
            switch(right.type) {
                case SHARD_VAL_PATH:
                    len = left.pathlen + right.pathlen;
                    sum = shard_gc_malloc(e->gc, (len + 1) * sizeof(char));
                    sum[0] = '\0';
                    strcat(sum, left.path);
                    strcat(sum, right.path);
                    break;
                case SHARD_VAL_STRING:
                    len = left.pathlen + right.strlen;
                    if(right.string[0] != '/')
                        len++;
                    sum = shard_gc_malloc(e->gc, (len + 1) * sizeof(char));
                    sum[0] = '\0';
                    strcat(sum, left.path);
                    if(right.string[0] != '/')
                        strcat(sum, "/");
                    strcat(sum, right.string);
                    break;
                default:
                    goto fail;
            }
            return PATH_VAL(sum, len);
        }
        
        default:
            is_left = true;
            break;
    }

fail:
    shard_eval_throw(e, *loc, "`+`: %s operand is not of a numeric type", is_left ? "left" : "right");
}

#define ARITH_OP_INT_FLT(op) \
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
    shard_eval_throw(e, *loc, "`" #op "`: %s operand is not of a numeric type", left_err ? "left" : "right"); \

struct shard_value shard_eval_subtraction(volatile struct shard_evaluator* e, struct shard_value left, struct shard_value right, struct shard_location* loc) {
    ARITH_OP_INT_FLT(-)
}

static inline struct shard_value eval_subtraction(volatile struct shard_evaluator* e, struct shard_expr* expr) {
    return shard_eval_subtraction(
        e,
        eval(e, expr->binop.lhs),
        eval(e, expr->binop.rhs),
        &expr->loc
    );
}

struct shard_value shard_eval_multiplication(volatile struct shard_evaluator* e, struct shard_value left, struct shard_value right, struct shard_location* loc) {
    ARITH_OP_INT_FLT(*)
}

static inline struct shard_value eval_multiplication(volatile struct shard_evaluator* e, struct shard_expr* expr) {
    return shard_eval_multiplication(
        e,
        eval(e, expr->binop.lhs),
        eval(e, expr->binop.rhs),
        &expr->loc
    );
}

struct shard_value shard_eval_division(volatile struct shard_evaluator* e, struct shard_value left, struct shard_value right, struct shard_location* loc) {
    ARITH_OP_INT_FLT(/)
}

static inline struct shard_value eval_division(volatile struct shard_evaluator* e, struct shard_expr* expr) {
    return shard_eval_division(
        e,
        eval(e, expr->binop.lhs),
        eval(e, expr->binop.rhs),
        &expr->loc
    );
}

static inline struct shard_value eval_negation(volatile struct shard_evaluator* e, struct shard_expr* expr) {
    struct shard_value value = eval(e, expr->unaryop.expr);
    switch(value.type) {
        case SHARD_VAL_INT:
            return INT_VAL(-value.integer);
        case SHARD_VAL_FLOAT:
            return FLOAT_VAL(-value.floating);
        default:
            shard_eval_throw(e, expr->loc, "`-`: operand is not of a numeric type");
    }
}

static inline struct shard_value eval_concat(volatile struct shard_evaluator* e, struct shard_expr* expr) {
    struct shard_value values[] = {
        eval(e, expr->binop.lhs),
        eval(e, expr->binop.rhs)
    };

    if(values[0].type != SHARD_VAL_LIST)
        shard_eval_throw(e, expr->loc, "`++`: left operand is not a list");

    if(values[1].type != SHARD_VAL_LIST)
        shard_eval_throw(e, expr->loc, "`++`: right operand is not a list");

    struct shard_list* last = values[0].list.head;
    if(!last)
        return values[1];

    while(last->next)
        last = last->next;

    last->next = values[1].list.head;

    return values[0];
}

static inline struct shard_value eval_merge(volatile struct shard_evaluator* e, struct shard_expr* expr) {
    struct shard_value values[] = {
        eval(e, expr->binop.lhs),
        eval(e, expr->binop.rhs)
    };

    if(values[0].type != SHARD_VAL_SET)
        shard_eval_throw(e, expr->loc, "`//`: left operand is not a set");

    if(values[1].type != SHARD_VAL_SET)
        shard_eval_throw(e, expr->loc, "`//`: right operand is not a set");

    struct shard_set* set = shard_set_merge(e->ctx, values[0].set, values[1].set);
    if(!set)
        shard_eval_throw(e, expr->loc, "`//`: %s", strerror(errno));

    shard_update_set_scopes(e, set, values[0].set);
    shard_update_set_scopes(e, set, values[1].set);

    return SET_VAL(set);
}

static inline struct shard_lazy_value* eval_get_attr(volatile struct shard_evaluator* e, struct shard_lazy_value* set, struct shard_attr_path* path, struct shard_location loc) {
    for(size_t i = 0; i < path->count; i++) {
        eval_lazy(e, set);
        
        if(set->eval.type != SHARD_VAL_SET)
            shard_eval_throw(e, loc, "`.%s` is not of type set", path->items[i]);

        int err = shard_set_get(set->eval.set, path->items[i], &set);
        if(err || !set)
            return NULL;
    }

    return set;
}

static inline struct shard_value eval_attr_test(volatile struct shard_evaluator* e, struct shard_expr* expr) {
    return BOOL_VAL(eval_get_attr(e, shard_lazy(e->ctx, expr->attr_test.set, e->scope), &expr->attr_test.path, expr->loc) != 0);
}

static inline struct shard_value eval_attr_sel(volatile struct shard_evaluator* e, struct shard_expr* expr) {
    struct shard_lazy_value* value = eval_get_attr(e, shard_lazy(e->ctx, expr->attr_test.set, e->scope), &expr->attr_test.path, expr->loc);
    if(value)
        return eval_lazy(e, value);

    if(expr->attr_sel.default_value)
        return eval(e, expr->attr_sel.default_value);

    shard_eval_throw(e, expr->loc, "`.`: set does not have this attribute");
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
            return value.set->size != 0;
        case SHARD_VAL_FUNCTION:
            return true;
        default:
            return false;
    }
}

static inline struct shard_value eval_logand(volatile struct shard_evaluator* e, struct shard_expr* expr) {
    return BOOL_VAL(is_truthy(eval(e, expr->binop.lhs)) && is_truthy(eval(e, expr->binop.rhs)));
}

static inline struct shard_value eval_logor(volatile struct shard_evaluator* e, struct shard_expr* expr) {
    return BOOL_VAL(is_truthy(eval(e, expr->binop.lhs)) || is_truthy(eval(e, expr->binop.rhs)));
}

static inline struct shard_value eval_logimpl(volatile struct shard_evaluator* e, struct shard_expr* expr) {
    return BOOL_VAL(!is_truthy(eval(e, expr->binop.lhs)) || is_truthy(eval(e, expr->binop.rhs)));
}

static inline struct shard_value eval_not(volatile struct shard_evaluator* e, struct shard_expr* expr) {
    return BOOL_VAL(!is_truthy(eval(e, expr->unaryop.expr)));
}

static inline struct shard_value eval_ternary(volatile struct shard_evaluator* e, struct shard_expr* expr) {
    return is_truthy(eval(e, expr->ternary.cond)) ? eval(e, expr->ternary.if_branch) : eval(e, expr->ternary.else_branch);
}

static inline struct shard_value eval_assert(volatile struct shard_evaluator* e, struct shard_expr* expr) {
    if(is_truthy(eval(e, expr->binop.lhs)))
        return eval(e, expr->binop.rhs);

    shard_eval_throw(e, expr->loc, "assertion failed");
}

static inline struct shard_value eval_ident(volatile struct shard_evaluator* e, struct shard_expr* expr) {
    struct shard_lazy_value* val = lookup_binding(e, expr->ident);
    if(!val)
        shard_eval_throw(e, expr->loc, "identifier `%s` not bound in this scope", expr->ident);

    return shard_value_copy(e, eval_lazy(e, val));
}

static inline struct shard_value eval_let(volatile struct shard_evaluator* e, struct shard_expr* expr) {
    struct shard_set* bindings = shard_set_init(e->ctx, expr->let.bindings.count);
    scope_push(e, bindings);
    
    for(size_t i = 0; i < expr->let.bindings.count; i++)
        shard_set_put(bindings, expr->let.bindings.items[i].ident, shard_lazy(e->ctx, expr->let.bindings.items[i].value, e->scope));
    
    struct shard_value value = eval(e, expr->let.expr);
    scope_pop(e);
    return value;
}

static inline struct shard_value eval_with(volatile struct shard_evaluator* e, struct shard_expr* expr) {
    struct shard_value set = eval(e, expr->binop.lhs);
    if(set.type != SHARD_VAL_SET)
        shard_eval_throw(e, expr->loc, "`with`: expect left operand to be of type set");

    scope_push(e, set.set);
    struct shard_value value = eval(e, expr->binop.rhs);
    scope_pop(e);
    return value;
}

static inline struct shard_value eval_call_function(volatile struct shard_evaluator* e, struct shard_value func, struct shard_lazy_value* arg_value, struct shard_location loc) {   
    assert(func.type == SHARD_VAL_FUNCTION);

    struct shard_set* set;
    struct shard_value arg;
    if(func.function.arg->type_constraint != SHARD_VAL_ANY) {
        arg = eval_lazy(e, arg_value);
        if(!(arg.type & func.function.arg->type_constraint))
            shard_eval_throw(e,
                    loc,
                    "function expected argument of type `%s`, but got `%s`",
                    shard_value_type_to_string(e->ctx, func.function.arg->type_constraint),
                    shard_value_type_to_string(e->ctx, arg.type)
            );
    }

    switch(func.function.arg->type) {
        case SHARD_PAT_IDENT:
            set = shard_set_init(e->ctx, 1);
            shard_set_put(set, func.function.arg->ident, arg_value);
            break;
        case SHARD_PAT_SET:
            set = shard_set_init(e->ctx, func.function.arg->attrs.count + (size_t) !!func.function.arg->ident);

            size_t expected_arity = arg.set->size;
            for(size_t i = 0; i < func.function.arg->attrs.count; i++) {
                struct shard_lazy_value* val;
                int err = shard_set_get(arg.set, func.function.arg->attrs.items[i].ident, &val);
                if(err || !val) {
                    if(func.function.arg->attrs.items[i].value) {
                        val = shard_lazy(e->ctx, func.function.arg->attrs.items[i].value, e->scope);
                        expected_arity++;
                    } 
                    else
                        shard_eval_throw(e, loc, "set does not contain expected attribute `%s`", func.function.arg->attrs.items[i].ident);
                }

                shard_set_put(set, func.function.arg->attrs.items[i].ident, val);
            }

            if(!func.function.arg->ellipsis && func.function.arg->attrs.count != expected_arity)
                shard_eval_throw(e, loc, "set does not match expected attributes");

            if(func.function.arg->ident)
                shard_set_put(set, func.function.arg->ident, shard_unlazy(e->ctx, SET_VAL(set)));

            break;
        default:
            assert(false && "unreachable");
    }

    struct shard_scope* prev_scope = e->scope;
    e->scope = func.function.scope;

    scope_push(e, set);
    struct shard_value value = eval(e, func.function.body);
    scope_pop(e);

    e->scope = prev_scope;

/*    switch(func.function.arg->type) {
        case SHARD_PAT_IDENT:
            *arg_value = set->entries[0].value;
            break;
        case SHARD_PAT_SET:
            for(size_t i = 0; i < set->capacity; i++) {
                if(set->entries[i].key && set->entries[i].key != func.function.arg->ident)
                    shard_set_put(arg.set, set->entries[i].key, set->entries[i].value);
            }
            break;
    }*/

    return value;
}

static struct shard_value eval_call_builtin(volatile struct shard_evaluator* e, struct shard_value value, struct shard_lazy_value* arg) {
    assert(value.type == SHARD_VAL_BUILTIN);

    if(value.builtin.builtin->arity == 1)
        return value.builtin.builtin->callback(e, value.builtin.builtin, &arg);
    else if(!value.builtin.num_queued_args)
        value.builtin.queued_args = shard_gc_malloc(e->gc, sizeof(void*) * value.builtin.builtin->arity);

    value.builtin.queued_args[value.builtin.num_queued_args++] = arg;
    if(value.builtin.num_queued_args != value.builtin.builtin->arity)
        return value;

    return value.builtin.builtin->callback(e, value.builtin.builtin, value.builtin.queued_args);
}

static struct shard_value eval_call_functor_set(volatile struct shard_evaluator* e, struct shard_value set, struct shard_lazy_value* arg, struct shard_location loc); 

static inline struct shard_value eval_call_value(volatile struct shard_evaluator* e, struct shard_value value, struct shard_lazy_value* arg, struct shard_location loc) {
    struct shard_error_scope error_scope = {
        .prev = e->error_scope,
        .loc = loc
    };
    e->error_scope = &error_scope;

    struct shard_value ret_value;
    switch(value.type) {
        case SHARD_VAL_FUNCTION:
             ret_value = eval_call_function(e, value, arg, loc);
             break;
        case SHARD_VAL_SET:
             ret_value = eval_call_functor_set(e, value, arg, loc);
             break;
        case SHARD_VAL_BUILTIN:
             ret_value = eval_call_builtin(e, value, arg);
             break;
        default:
            shard_eval_throw(e, loc, "attempt to call something which is not a function");
    }

    e->error_scope = e->error_scope->prev;
    return ret_value;
}

SHARD_DECL struct shard_value shard_eval_call(volatile struct shard_evaluator* e, struct shard_value value, struct shard_lazy_value* arg, struct shard_location loc) {
    return eval_call_value(e, value, arg, loc);
}

static inline struct shard_value eval_call(volatile struct shard_evaluator* e, struct shard_expr* expr) {
    return eval_call_value(e, eval(e, expr->binop.lhs), shard_lazy(e->ctx, expr->binop.rhs, e->scope), expr->loc);
}

static struct shard_value eval_call_functor_set(volatile struct shard_evaluator* e, struct shard_value set, struct shard_lazy_value* arg, struct shard_location loc) {
    assert(set.type == SHARD_VAL_SET);

    struct shard_lazy_value* functor;
    int err = shard_set_get(set.set, shard_get_ident(e->ctx, "__functor"), &functor);
    if(err || !functor)
        shard_eval_throw(e, loc, "attempt to call set without a `__functor` attribute");

    return eval_call_value(e, eval_call_value(e, eval_lazy(e, functor), shard_unlazy(e->ctx, set), loc), arg, loc);
}

static struct shard_value eval(volatile struct shard_evaluator* e, struct shard_expr* expr) {
    switch(expr->type) {
        case SHARD_EXPR_INT:
            return INT_VAL(expr->integer);
        case SHARD_EXPR_FLOAT:
            return FLOAT_VAL(expr->floating);
        case SHARD_EXPR_STRING:
            return STRING_VAL(expr->string, strlen(expr->string));
        case SHARD_EXPR_PATH:
            return eval_path(e, expr);
        case SHARD_EXPR_LIST:
            return eval_list(e, expr);
        case SHARD_EXPR_SET:
            return eval_set(e, expr);
        case SHARD_EXPR_EQ:
            return eval_eq(e, expr, false);
        case SHARD_EXPR_NE:
            return eval_eq(e, expr, true);
        case SHARD_EXPR_LE:
            return eval_le(e, expr);
        case SHARD_EXPR_LT:
            return eval_lt(e, expr);
        case SHARD_EXPR_GE:
            return eval_ge(e, expr);
        case SHARD_EXPR_GT:
            return eval_gt(e, expr);
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
        case SHARD_EXPR_CONCAT:
            return eval_concat(e, expr);
        case SHARD_EXPR_MERGE:
            return eval_merge(e, expr);
        case SHARD_EXPR_ATTR_TEST:
            return eval_attr_test(e, expr);
        case SHARD_EXPR_ATTR_SEL:
            return eval_attr_sel(e, expr);
        case SHARD_EXPR_ASSERT:
            return eval_assert(e, expr);
        case SHARD_EXPR_FUNCTION:
            return eval_function(e, expr);
        case SHARD_EXPR_LET:
            return eval_let(e, expr);
        case SHARD_EXPR_IDENT:
            return eval_ident(e, expr);
        case SHARD_EXPR_WITH:
            return eval_with(e, expr);
        case SHARD_EXPR_CALL:
            return eval_call(e, expr);
        case SHARD_EXPR_BUILTIN:
            return expr->builtin.callback(e);
        default:
            shard_eval_throw(e, expr->loc, "unimplemented expression `%d`.", expr->type);
    }
}

int shard_eval_lazy(struct shard_context* ctx, struct shard_lazy_value* value) {
    if(value->evaluated)
        return 0;

    jmp_buf exception;
    volatile struct shard_evaluator e;
    evaluator_init(&e, ctx, &exception);

    if(setjmp(*e.exception) == SHARD_EVAL_OK) {
        e.scope = value->scope;
        value->eval = eval(&e, value->lazy);
        value->evaluated = true;
    }

    evaluator_free(&e);

    return ctx->errors.count;
}

int shard_eval(struct shard_context* ctx, struct shard_open_source* source) {
    if(!source->parsed) {
        int err = shard_parse(ctx, &source->source, &source->expr);
        if(err)
            goto finish;

        source->parsed = true;
    }

    if(!source->evaluated) {
        jmp_buf exception;
        volatile struct shard_evaluator e;
        evaluator_init(&e, ctx, &exception);

        if(!ctx->builtin_initialized)
            shard_get_builtins(ctx, &ctx->builtin_scope);

        e.scope = &ctx->builtin_scope;

        if(setjmp(*e.exception) == SHARD_EVAL_OK)
            source->result = eval(&e, &source->expr);

        evaluator_free(&e);

        source->evaluated = true;
    }
finish:
    return ctx->errors.count;
}

SHARD_DECL int shard_call(struct shard_context* ctx, struct shard_value func, struct shard_value* arg, struct shard_value* result) {
    jmp_buf exception;
    volatile struct shard_evaluator e;
    evaluator_init(&e, ctx, &exception);

    if(setjmp(*e.exception) == SHARD_EVAL_OK) {
        e.scope = &ctx->builtin_scope;

        *result = eval_call_value(&e, func, shard_unlazy(ctx, *arg), (struct shard_location){.src=NULL,.line=1,.offset=0,.width=1});
    }

    evaluator_free(&e);

    return ctx->errors.count; 
}

SHARD_DECL void shard_update_set_scopes(volatile struct shard_evaluator* e, struct shard_set* new, struct shard_set* old) {
    struct shard_scope* new_scope = scope_init(e, new);

    for(size_t i = 0; i < new->size; i++) {
        struct shard_lazy_value* entr = new->entries[i].value;
        if(!new->entries[i].key || entr->evaluated || entr->scope->bindings != old)
            continue;
        
        new_scope->outer = entr->scope->outer;

        new->entries[i].value = shard_lazy(e->ctx, entr->lazy, new_scope);
    }
}

