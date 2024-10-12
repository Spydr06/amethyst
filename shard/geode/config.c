#include "libshard-util.h"
#include <config.h>

#include <geode.h>
#include <context.h>
#include <libshard.h>

#include <unistd.h>
#include <memory.h>

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

static void get_loader(struct geode_context* ctx, struct open_shard_source* open) {
    size_t loader_size = strlen(loader_fmt) + strlen(ctx->store_path) + strlen(ctx->main_config_path) + strlen(GEODE_PRELUDE_FILE) + 1;

    char* loader = geode_calloc(ctx, loader_size, sizeof(char));
    snprintf(loader, loader_size, loader_fmt, ctx->store_path, GEODE_PRELUDE_FILE, ctx->main_config_path);

    int err = shard_string_source(&ctx->shard_ctx, &open->source, __FILE_NAME__ "/<loader>", loader, strlen(loader), 0);
    if(err)
        geode_throw(ctx, FILE_IO, .file=TUPLE("loader source", err));

    open->open = true;
}

static void compile_configuration(struct geode_context* ctx, struct open_shard_source* loader) {
    int err = shard_eval(&ctx->shard_ctx, &loader->source, &loader->result, &loader->expr);
    if(err)
        geode_throw(ctx, SHARD, .shard=TUPLE(shard_get_errors(&ctx->shard_ctx), err));
}

void geode_open_source(struct geode_context* ctx, const char* path, struct open_shard_source* open) {
    memset(open, 0, sizeof(struct open_shard_source));

    int err = geode_open_shard_file(path, &open->source, "rb");
    if(err)
        geode_throw(ctx, FILE_IO, .file=TUPLE(path, err));

    open->open = true;

    err = shard_eval(&ctx->shard_ctx, &open->source, &open->result, &open->expr);
    if(err)
        geode_throw(ctx, SHARD, .shard=TUPLE(shard_get_errors(&ctx->shard_ctx), err));
}

void geode_close_source(struct geode_context* ctx, struct open_shard_source* open) {
    if(open->open) {
        shard_free_expr(&ctx->shard_ctx, &open->expr);
        open->source.close(&open->source);
        open->open = false;
    }
}

void geode_load_config(struct geode_context* ctx) {
    get_loader(ctx, &ctx->configuration);
    compile_configuration(ctx, &ctx->configuration);

    infof(ctx, "Loaded configuration `%s`.\n", ctx->main_config_path);
}


