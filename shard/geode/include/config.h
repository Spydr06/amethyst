#ifndef _GEODE_CONFIG_H
#define _GEODE_CONFIG_H

#include <libshard.h>

struct geode_context;

#define GEODE_DEFAULT_CONFIG_FILE "etc/configuration.shard"
#define GEODE_DEFAULT_STORE_PATH "etc/geode/store"
#define GEODE_PRELUDE_FILE "prelude.shard"
#define GEODE_DEFAULT_PREFIX "/"

void geode_load_config(struct geode_context* ctx);
bool geode_config_loaded(struct geode_context* ctx);

struct shard_value* geode_get_config_attr(struct geode_context* ctx, char* attr_name);

#endif /* _GEODE_CONFIG_H */

