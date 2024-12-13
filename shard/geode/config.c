#include "fs_util.h"
#include <config.h>

#include <geode.h>
#include <context.h>
#include <libshard.h>

#include <assert.h>
#include <unistd.h>
#include <memory.h>
#include <stdlib.h>

static struct shard_open_source* prelude(struct geode_context* ctx) {
    const char* parts[] = {
        ctx->store_path,
        GEODE_PRELUDE_FILE,
        NULL
    };

    char* prelude_file;
    assert(geode_concat_paths(ctx, &prelude_file, NULL, parts) == 0);

    return shard_open(&ctx->shard_ctx, prelude_file);
}

static void compile_configuration(struct geode_context* ctx, struct shard_open_source* loader) {
    int err = shard_eval(&ctx->shard_ctx, loader);
    if(err)
        geode_throw(ctx, SHARD, .shard=TUPLE(shard_get_errors(&ctx->shard_ctx), err));

    ctx->configuration = &loader->result;
}

static void verify_configuration(struct geode_context* ctx) {
    assert(geode_config_loaded(ctx));

    if(ctx->configuration->type != SHARD_VAL_SET)
        geode_throw(ctx, CONFIGURATION, .config=TUPLE(ctx->configuration, "function in `configuration.shard` must return a set."));
}

void geode_load_config(struct geode_context* ctx) {
    if(geode_config_loaded(ctx))
        return;

    compile_configuration(ctx, prelude(ctx));
    verify_configuration(ctx);

    infof(ctx, "Loaded configuration `%s`.\n", ctx->main_config_path);
}

bool geode_config_loaded(struct geode_context* ctx) {
    return ctx->configuration;
}

struct shard_value* geode_get_config_attr(struct geode_context* ctx, char* attr_name) {
    if(!geode_config_loaded(ctx))
        return NULL;

    struct shard_value* cur_set = ctx->configuration;
    
    char* token;
    while((token = strtok(attr_name, "."))) {
        struct shard_lazy_value* attr;

        if(cur_set->type != SHARD_VAL_SET) {
            assert(false); // TODO: print error
        }

        if(shard_set_get(cur_set->set, token, &attr)) {
            assert(false); // TODO: print error
        }

        int err = shard_eval_lazy(&ctx->shard_ctx, attr);
        if(err)
            geode_throw(ctx, SHARD, .shard=TUPLE(shard_get_errors(&ctx->shard_ctx), err));
    
        cur_set = &attr->eval;
    }

    return cur_set;
}

