#ifndef _GEODE_BUILDCHAIN_H
#define _GEODE_BUILDCHAIN_H

#include "context.h"

#include <libshard.h>

struct geode_buildchain {
    struct shard_hashmap tools;
};

void buildchain_setup_cross_env(struct geode_context* ctx, struct geode_buildchain* cross_chain);

void buildchain_setenv(struct geode_context* ctx, struct geode_buildchain* buildchain);

#endif /* _GEODE_BUILDCHAIN_H */

