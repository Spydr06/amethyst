#define _LIBSHARD_INTERNAL
#include <libshard.h>

#include <time.h>
#include <stdio.h>

#define BUILTIN_EXPR(_callback) ((struct shard_expr) {.type = SHARD_EXPR_BUILTIN, .loc = {0}, .builtin.callback = (_callback) })

struct builtin {
    const char* const ident;
    struct shard_lazy_value value;
};

static struct shard_value builtin_abort(volatile struct shard_evaluator* e, struct shard_lazy_value** args, struct shard_location* loc) {
    struct shard_value arg = shard_eval_lazy2(e, *args);
    if(arg.type != SHARD_VAL_STRING)
        shard_eval_throw(e, *loc, "`builtins.abort` expects the argument to be of type `string`");

    shard_eval_throw(e, *loc, "evaluation aborted with the following message: '%.*s'", (int) arg.strlen, arg.string);
}

static struct shard_value builtin_attrNames(volatile struct shard_evaluator* e, struct shard_lazy_value** args, struct shard_location* loc) {
    struct shard_value arg = shard_eval_lazy2(e, *args);
    if(arg.type != SHARD_VAL_SET)
        shard_eval_throw(e, *loc, "`builtins.attrNames` expects the argument to be of type `set`");

    struct shard_list *head = NULL, *current = NULL, *next;
    for(size_t i = 0; i < arg.set->capacity; i++) {
        if(!arg.set->entries[i].key)
            continue;

        next = shard_gc_malloc(e->gc, sizeof(struct shard_list));
        next->value = UNLAZY_VAL(CSTRING_VAL(arg.set->entries[i].key));
        next->next = NULL;

        if(current)
            current->next = next;
        current = next;
        if(!head)
            head = current;
    }

    // TODO: sort alphabetically
    return LIST_VAL(head);
}

static struct shard_value builtin_attrValues(volatile struct shard_evaluator* e, struct shard_lazy_value** args, struct shard_location* loc) {
    struct shard_value arg = shard_eval_lazy2(e, *args);
    if(arg.type != SHARD_VAL_SET)
        shard_eval_throw(e, *loc, "`builtins.attrValues` expects the argument to be of type `set`");

    struct shard_list *head = NULL, *current = NULL, *next;
    for(size_t i = 0; i < arg.set->capacity; i++) {
        if(!arg.set->entries[i].key)
            continue;

        next = shard_gc_malloc(e->gc, sizeof(struct shard_list));
        next->value = arg.set->entries[i].value;
        next->next = NULL;

        if(current)
            current->next = next;
        current = next;
        if(!head)
            head = current;
    }

    // TODO: sort after keys alphabetically
    return LIST_VAL(head);
}

static struct shard_value builtin_isAttrs(volatile struct shard_evaluator* e, struct shard_lazy_value** args, struct shard_location* loc) {
    (void) loc;
    struct shard_value arg = shard_eval_lazy2(e, *args);
    return BOOL_VAL(arg.type == SHARD_VAL_SET);
}

static struct shard_value builtin_isBool(volatile struct shard_evaluator* e, struct shard_lazy_value** args, struct shard_location* loc) {
    (void) loc;
    struct shard_value arg = shard_eval_lazy2(e, *args);
    return BOOL_VAL(arg.type == SHARD_VAL_BOOL);
}

static struct shard_value builtin_isFloat(volatile struct shard_evaluator* e, struct shard_lazy_value** args, struct shard_location* loc) {
    (void) loc;
    struct shard_value arg = shard_eval_lazy2(e, *args);
    return BOOL_VAL(arg.type == SHARD_VAL_FLOAT);
}

static struct shard_value builtin_isFunction(volatile struct shard_evaluator* e, struct shard_lazy_value** args, struct shard_location* loc) {
    (void) loc;
    struct shard_value arg = shard_eval_lazy2(e, *args);
    return BOOL_VAL(arg.type & (SHARD_VAL_FUNCTION | SHARD_VAL_BUILTIN));
}

static struct shard_value builtin_isInt(volatile struct shard_evaluator* e, struct shard_lazy_value** args, struct shard_location* loc) {
    (void) loc;
    struct shard_value arg = shard_eval_lazy2(e, *args);
    return BOOL_VAL(arg.type == SHARD_VAL_INT);
}

static struct shard_value builtin_isList(volatile struct shard_evaluator* e, struct shard_lazy_value** args, struct shard_location* loc) {
    (void) loc;
    struct shard_value arg = shard_eval_lazy2(e, *args);
    return BOOL_VAL(arg.type == SHARD_VAL_LIST);
}

static struct shard_value builtin_isNull(volatile struct shard_evaluator* e, struct shard_lazy_value** args, struct shard_location* loc) {
    (void) loc;
    struct shard_value arg = shard_eval_lazy2(e, *args);
    return BOOL_VAL(arg.type == SHARD_VAL_NULL);
}

static struct shard_value builtin_isPath(volatile struct shard_evaluator* e, struct shard_lazy_value** args, struct shard_location* loc) {
    (void) loc;
    struct shard_value arg = shard_eval_lazy2(e, *args);
    return BOOL_VAL(arg.type == SHARD_VAL_PATH);
}

static struct shard_value builtin_isString(volatile struct shard_evaluator* e, struct shard_lazy_value** args, struct shard_location* loc) {
    (void) loc;
    struct shard_value arg = shard_eval_lazy2(e, *args);
    return BOOL_VAL(arg.type == SHARD_VAL_STRING);
}

static struct shard_value builtin_length(volatile struct shard_evaluator* e, struct shard_lazy_value** args, struct shard_location* loc) {
    struct shard_value arg = shard_eval_lazy2(e, *args);
    if(arg.type != SHARD_VAL_LIST)
        shard_eval_throw(e, *loc, "`builtins.length` expects list argument");

    int64_t i = 0;
    struct shard_list* current = arg.list.head;
    while(current) {
        current = current->next;
        i++;
    }

    return INT_VAL(i);
}

static struct shard_value builtin_toString(volatile struct shard_evaluator* e, struct shard_lazy_value** args, struct shard_location* loc) {
    struct shard_value arg = shard_eval_lazy2(e, *args);
    switch(arg.type) {
        case SHARD_VAL_STRING:
            return arg;
        case SHARD_VAL_NULL:
            return CSTRING_VAL("");
        case SHARD_VAL_PATH:
            return STRING_VAL(arg.path, arg.pathlen);
        case SHARD_VAL_BOOL:
            return arg.boolean ? CSTRING_VAL("1") : CSTRING_VAL("");
        case SHARD_VAL_INT: {
            char* buf = shard_gc_malloc(e->gc, 32);
            snprintf(buf, 32, "%ld", arg.integer);
            return CSTRING_VAL(buf);
        } break;
        // TODO: lists
        case SHARD_VAL_SET: {
            struct shard_lazy_value* val;
            if(shard_set_get(arg.set, shard_get_ident(e->ctx, "__toString"), &val))
                shard_eval_throw(e, *loc, "`builtins.toString` cannot print set without `__toString` attribute");
            
            struct shard_value to_str = shard_eval_lazy2(e, val);
            if(!(to_str.type & (SHARD_VAL_BUILTIN | SHARD_VAL_FUNCTION)))
                shard_eval_throw(e, *loc, "`builtins.toString` `__toString` attribute must be of type function");

            struct shard_value returned = shard_eval_call(e, to_str, &UNLAZY_VAL(arg), *loc);
            if(returned.type != SHARD_VAL_STRING)
                shard_eval_throw(e, *loc, "`builtins.toString` `__toString` must return a string");

            return returned;
        } break;
        default:
            shard_eval_throw(e, *loc, "`builtins.toString` cannot convert this value to string");
    }
}

static struct shard_value builtin_tryEval(volatile struct shard_evaluator* e, struct shard_lazy_value** args, struct shard_location* loc) {
    shard_eval_lazy(e->ctx, *args);

    struct shard_value success, value;
    if((*args)->evaluated) {
        success = BOOL_VAL(true);
        value = (*args)->eval;
    }
    else
        value = success = BOOL_VAL(false);

    struct shard_set* set = shard_set_init(e->ctx, 2);
    shard_set_put(set, shard_get_ident(e->ctx, "success"), UNLAZY_VAL(success));
    shard_set_put(set, shard_get_ident(e->ctx, "value"), UNLAZY_VAL(value));

    return SET_VAL(set);
}

static struct shard_value builtin_typeOf(volatile struct shard_evaluator* e, struct shard_lazy_value** args, struct shard_location* loc) {
    struct shard_value val = shard_eval_lazy2(e, *args);

    switch(val.type) {
        case SHARD_VAL_INT:
            return CSTRING_VAL("int");
        case SHARD_VAL_BOOL:
            return CSTRING_VAL("bool");
        case SHARD_VAL_FLOAT:
            return CSTRING_VAL("float");
        case SHARD_VAL_STRING:
            return CSTRING_VAL("string");
        case SHARD_VAL_PATH:
            return CSTRING_VAL("path");
        case SHARD_VAL_NULL:
            return CSTRING_VAL("null");
        case SHARD_VAL_SET:
            return CSTRING_VAL("set");
        case SHARD_VAL_LIST:
            return CSTRING_VAL("list");
        case SHARD_VAL_FUNCTION:
        case SHARD_VAL_BUILTIN:
            return CSTRING_VAL("lambda");
    }
}

static struct shard_value builtin_head(volatile struct shard_evaluator* e, struct shard_lazy_value** args, struct shard_location* loc) {
    struct shard_value val = shard_eval_lazy2(e, *args);
    if(val.type != SHARD_VAL_LIST)
        shard_eval_throw(e, *loc, "`builtins.head` expects a list");

    if(!val.list.head)
        return NULL_VAL();
    return shard_eval_lazy2(e, &val.list.head->value);
}

static struct shard_value builtin_tail(volatile struct shard_evaluator* e, struct shard_lazy_value** args, struct shard_location* loc) {
    struct shard_value val = shard_eval_lazy2(e, *args);
    if(val.type != SHARD_VAL_LIST)
        shard_eval_throw(e, *loc, "`builtins.tail` expects a list");

    if(!val.list.head)
        return LIST_VAL(NULL);
    return LIST_VAL(val.list.head->next);
}

static struct shard_value builtin_throw(struct shard_evaluator* eval, struct shard_location* loc, struct shard_value arg) {
    if(arg.type != SHARD_VAL_STRING)
        shard_eval_throw(eval, *loc, "`builtins.throw` expects the argument to be of type `string`");

    shard_eval_throw(eval, *loc, "%.*s", (int) arg.strlen, arg.string);
}

static struct shard_value builtin_import(struct shard_evaluator* e, struct shard_location* loc, struct shard_value arg) {
    if(arg.type != SHARD_VAL_PATH)
        shard_eval_throw(e, *loc, "`builtins.import` expects the argument to be of type `path`");

    return (struct shard_value){.type = SHARD_VAL_NULL};
}

static struct shard_value builtin_currentTime(volatile struct shard_evaluator* e) {
    (void) e;
    return INT_VAL(time(NULL));
}

static struct shard_value builtin_shardPath(volatile struct shard_evaluator* e) {
    struct shard_list *head = NULL, *current = NULL, *next;
    
    for(size_t i = 0; i < e->ctx->include_dirs.count; i++) {
        next = shard_gc_malloc(e->gc, sizeof(struct shard_list));
        next->next = NULL;
        next->value = UNLAZY_VAL(CPATH_VAL(e->ctx->include_dirs.items[i]));

        if(current)
            current->next = next;
        current = next;
        if(!head)
            head = current;
    }

    return LIST_VAL(head);
}

void shard_get_builtins(struct shard_context* ctx, struct shard_scope* dest) {
    static struct shard_expr currentTime = BUILTIN_EXPR(builtin_currentTime),
                             shardPath   = BUILTIN_EXPR(builtin_shardPath);

    struct builtin builtins[] = {
        { "shardVersion", UNLAZY_VAL(CSTRING_VAL(SHARD_VERSION)) },
        { "langVersion", UNLAZY_VAL(INT_VAL(SHARD_VERSION_X)) },
        { "currentTime", LAZY_VAL(&currentTime, NULL) },
        { "shardPath", LAZY_VAL(&shardPath, NULL) },
        { "currentSystem", UNLAZY_VAL(ctx->current_system ? CSTRING_VAL(ctx->current_system) : NULL_VAL()) },
        { "abort", UNLAZY_VAL(BUILTIN_VAL(builtin_abort, 1)) },
        // add
        // all
        // any
        { "attrNames", UNLAZY_VAL(BUILTIN_VAL(builtin_attrNames, 1)) },
        { "attrValues", UNLAZY_VAL(BUILTIN_VAL(builtin_attrValues, 1)) },
        { "isAttrs", UNLAZY_VAL(BUILTIN_VAL(builtin_isAttrs, 1)) },
        { "isBool", UNLAZY_VAL(BUILTIN_VAL(builtin_isBool, 1)) },
        { "isFloat", UNLAZY_VAL(BUILTIN_VAL(builtin_isFloat, 1)) },
        { "isFunction", UNLAZY_VAL(BUILTIN_VAL(builtin_isFunction, 1)) },
        { "isInt", UNLAZY_VAL(BUILTIN_VAL(builtin_isInt, 1)) },
        { "isList", UNLAZY_VAL(BUILTIN_VAL(builtin_isList, 1)) },
        { "isNull", UNLAZY_VAL(BUILTIN_VAL(builtin_isNull, 1)) },
        { "isPath", UNLAZY_VAL(BUILTIN_VAL(builtin_isPath, 1)) },
        { "isString", UNLAZY_VAL(BUILTIN_VAL(builtin_isString, 1)) },
        { "length", UNLAZY_VAL(BUILTIN_VAL(builtin_length, 1)) },
        { "toString", UNLAZY_VAL(BUILTIN_VAL(builtin_toString, 1)) },
        { "tryEval", UNLAZY_VAL(BUILTIN_VAL(builtin_tryEval, 1)) },
        { "typeOf", UNLAZY_VAL(BUILTIN_VAL(builtin_typeOf, 1)) },
        { "head", UNLAZY_VAL(BUILTIN_VAL(builtin_head, 1)) },
        { "tail", UNLAZY_VAL(BUILTIN_VAL(builtin_tail, 1)) },
    };

    struct shard_set* builtin_set = shard_set_init(ctx, LEN(builtins));

    for(size_t i = 0; i < LEN(builtins); i++) {
        shard_set_put(builtin_set, shard_get_ident(ctx, builtins[i].ident), builtins[i].value);
    }

    struct shard_set* global_scope = shard_set_init(ctx, 4);
    shard_set_put(global_scope, shard_get_ident(ctx, "true"),     UNLAZY_VAL(BOOL_VAL(true)));
    shard_set_put(global_scope, shard_get_ident(ctx, "false"),    UNLAZY_VAL(BOOL_VAL(false)));
    shard_set_put(global_scope, shard_get_ident(ctx, "null"),     UNLAZY_VAL(NULL_VAL()));
    shard_set_put(global_scope, shard_get_ident(ctx, "builtins"), UNLAZY_VAL(SET_VAL(builtin_set)));

    dest->bindings = global_scope;
    dest->outer = NULL;
    
    ctx->builtin_intialized = true;    
}
