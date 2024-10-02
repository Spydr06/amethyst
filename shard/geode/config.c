#include <config.h>

#include <geode.h>
#include <context.h>
#include <libshard.h>

#include <unistd.h>
#include <memory.h>

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
    geode_open_source(ctx, ctx->main_config_path, &ctx->configuration);

    infof(ctx, "Loaded configuration `%s`.\n", ctx->main_config_path);
}


