#define _LIBSHARD_INTERNAL
#include <libshard.h>

#define BUILTIN_VAL(_callback) ((struct shard_value){ .type = SHARD_VAL_BUILTIN, .builtin = { .callback = (_callback) } })

static struct shard_value builtin_abort(struct shard_evaluator* eval, struct shard_location* loc, struct shard_value arg) {
    if(arg.type != SHARD_VAL_STRING)
        shard_eval_throw(eval, *loc, "`builtins.abort` expects the argument to be of type `string`");

    shard_eval_throw(eval, *loc, "evaluation aborted with the following message: '%.*s'", (int) arg.strlen, arg.string);
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

struct builtin {
    const char* const ident;
    struct shard_value value;
};

void shard_get_builtins(struct shard_context* ctx, struct shard_scope* dest) {
    struct builtin builtins[] = {
        { "true",  BOOL_VAL(true) },
        { "false", BOOL_VAL(false) },
        { "null",  NULL_VAL() },
    };

    struct shard_set* builtin_set = shard_set_init(ctx, LEN(builtins));

    for(size_t i = 0; i < LEN(builtins); i++) {
        shard_set_put(builtin_set, shard_get_ident(ctx, builtins[i].ident), UNLAZY_VAL(builtins[i].value));
    }

    dest->bindings = builtin_set;
    dest->outer = NULL;
}
