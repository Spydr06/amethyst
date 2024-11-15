#include <geode.h>
#include <context.h>
#include <config.h>

#include <libshard.h>

#include <assert.h>
#include <string.h>
#include <unistd.h>

static struct shard_value builtin_file_exists(volatile struct shard_evaluator* e, struct shard_lazy_value** args, struct shard_location* loc);

static const struct {
    const char* ident;
    struct shard_value (*callback)(volatile struct shard_evaluator*, struct shard_lazy_value**, struct shard_location*);
    unsigned arity;
} geode_builtin_functions[] = {
    {"geode.file.exists", builtin_file_exists, 1},
};

static void load_constants(struct geode_context* ctx) {
    struct {
        const char* ident;
        struct shard_value value;
    } builtin_constants[] = {
        {"geode.prefix", (struct shard_value){.type=SHARD_VAL_PATH, .path=ctx->prefix, .pathlen=strlen(ctx->prefix)}}
    };

    for(size_t i = 0; i < __len(builtin_constants); i++) {
        int err = shard_define_constant(
            &ctx->shard_ctx,
            builtin_constants[i].ident,
            builtin_constants[i].value
        );

        assert(err == 0 && "shard_define_constant returned error");
    }
}

void geode_load_builtins(struct geode_context* ctx) {
    load_constants(ctx);

    for(size_t i = 0; i < __len(geode_builtin_functions); i++) {
        int err = shard_define_function(
            &ctx->shard_ctx, 
            geode_builtin_functions[i].ident, 
            geode_builtin_functions[i].callback, 
            geode_builtin_functions[i].arity
        );

        assert(err == 0 && "shard_define_function returned error");
    }
}

static struct shard_value builtin_file_exists(volatile struct shard_evaluator* e, struct shard_lazy_value** args, struct shard_location* loc) {
    struct shard_value arg = shard_eval_lazy2(e, *args);
    if(arg.type != SHARD_VAL_PATH)
        shard_eval_throw(e, *loc, "`geode.file.exists` expects argument to be of type `string`");
    
    return (struct shard_value){.type=SHARD_VAL_BOOL, .boolean=access(arg.path, F_OK) == 0};
}

