#ifndef _SHARD_PACKAGE_H
#define _SHARD_PACKAGE_H

#include <libshard.h>

#include <stdint.h>

#include "context.h"

enum license : int8_t {
    LICENSE_UNKNOWN = -1,

    LICENSE_MIT,
    LICENSE_GPL,
    LICENSE_GPLv2,
    LICENSE_GPLv3,
    LICENSE_BSD,

    LICENSE_NONFREE = INT8_MAX
};

struct package_version {
    uint16_t x, y, z;
};

struct package_ref {
    uint32_t hash;
};

struct package_spec {
    const char* name;
    struct package_version version;
    uint32_t hash;

    enum license license;

    uint32_t num_deps;
    struct package_ref* deps; 

    // install pipeline
    struct shard_value fetch;
    struct shard_value patch;
    struct shard_value configure;
    struct shard_value build;
    struct shard_value check;
    struct shard_value install;
};

struct package_index {
    struct package_spec* packages;
    size_t num_packages;
};

int geode_index_packages(struct geode_context* ctx, struct package_index* index);

int geode_load_package(struct geode_context* ctx, struct package_spec* spec, struct shard_lazy_value* src);



#endif /* _SHARD_PACKAGE_H */

