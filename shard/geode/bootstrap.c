#include <bootstrap.h>
#include <geode.h>
#include <config.h>

int geode_bootstrap(struct geode_context* ctx, int argc, char** argv) {
    infof(ctx, "Bootstrapping `%s` to `%s`... (This could take a while)\n", ctx->main_config_path, ctx->prefix);    

    geode_load_config(ctx);    

    return 0;
}

