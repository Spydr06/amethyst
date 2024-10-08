#ifndef _GEODE_CONFIG_H
#define _GEODE_CONFIG_H

#include <libshard.h>

struct geode_context;

#define GEODE_DEFAULT_CONFIG_FILE "etc/configuration.shard"
#define GEODE_DEFAULT_PREFIX "/"

struct open_shard_source {
    struct shard_source source;
    struct shard_expr expr;
    struct shard_value result;
    bool open;
};

void geode_open_source(struct geode_context* ctx, const char* path, struct open_shard_source* open);
void geode_close_source(struct geode_context* ctx, struct open_shard_source* open);

void geode_load_config(struct geode_context* ctx);

#endif /* _GEODE_CONFIG_H */

