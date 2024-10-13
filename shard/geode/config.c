#include "libshard-util.h"
#include <config.h>

#include <geode.h>
#include <context.h>
#include <libshard.h>

#include <assert.h>
#include <unistd.h>
#include <memory.h>
#include <stdlib.h>

#define _(str) str

static const char loader_fmt[] = _("\n\
# Loads the prelude and configuration files, then applies the prelude to the configuration.\n\
#   prelude :: map\n\
#   config :: map -> map\n\
\n\
with builtins; let\n\
    prelude = import \"%s/%s\";\n\
    config = import \"%s\";\n\
in config prelude\n\
");

static struct shard_open_source* get_loader(struct geode_context* ctx) {
    struct shard_open_source* open = malloc(sizeof(struct shard_open_source));
    memset(open, 0, sizeof(struct shard_open_source));

    size_t loader_size = strlen(loader_fmt) + strlen(ctx->store_path) + strlen(ctx->main_config_path) + strlen(GEODE_PRELUDE_FILE) + 1;

    char* loader = geode_calloc(ctx, loader_size, sizeof(char));
    snprintf(loader, loader_size, loader_fmt, ctx->store_path, GEODE_PRELUDE_FILE, ctx->main_config_path);

    int err = shard_string_source(&ctx->shard_ctx, &open->source, __FILE_NAME__ "/<loader>", loader, strlen(loader), 0);
    if(err)
        geode_throw(ctx, FILE_IO, .file=TUPLE("loader source", err));

    open->opened = true;
    open->auto_close = true;
    open->auto_free = true;

    shard_register_open(&ctx->shard_ctx, "<loader>", false, open);

    return open;
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

    compile_configuration(ctx, get_loader(ctx));
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

