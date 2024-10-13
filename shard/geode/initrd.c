#include "config.h"
#include <geode.h>
#include <context.h>

void geode_generate_initrd(struct geode_context* ctx, const char* path) {
    infof(ctx, "Generating initrd (%s)...\n", path);

    //struct shard_value* initrd_config = geode_get_config_attr(ctx, LSTR("initrd"));
}

