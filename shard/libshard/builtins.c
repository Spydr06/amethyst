#define _LIBSHARD_INTERNAL
#include <libshard.h>

#include <time.h>

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

static struct shard_value builtin_throw(struct shard_evaluator* eval, struct shard_location* loc, struct shard_value arg) {
    if(arg.type != SHARD_VAL_STRING)
        shard_eval_throw(eval, *loc, "`builtins.throw` expects the argument to be of type `string`");

    shard_eval_throw(eval, *loc, "%.*s", (int) arg.strlen, arg.string);
}

static struct shard_value builtin_import(struct shard_evaluator* eval, struct shard_location* loc, struct shard_value arg) {
    if(arg.type != SHARD_VAL_PATH)
        shard_eval_throw(eval, *loc, "`builtins.import` expects the argument to be of type `path`");

    // TODO:
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
