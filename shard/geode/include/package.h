#ifndef _SHARD_PACKAGE_H
#define _SHARD_PACKAGE_H

#include "geode.h"
#include "lifetime.h"

typedef struct geode_package package_t;

struct geode_package_index {
    struct shard_hashmap index;
    lifetime_t lifetime;
};

void geode_mkindex(struct geode_package_index *index);
void geode_index_packages(struct geode_context *context, struct geode_package_index *index);

struct geode_version {
    int x, y, z, w;
    int commit;
};

struct geode_dependency {
    const char *name;

    bool has_min_constraint;
    bool has_max_constraint;
    struct geode_version min_version;
    struct geode_version max_version;
};

struct geode_package {
    const char *origin;
    
    const char *name;
    struct geode_version version;
    uint8_t hash[16];

    size_t comptime_deps_count, runtime_deps_count;
    struct geode_dependency *comptime_deps;
    struct geode_dependency *runtime_deps;

    struct shard_value fetch;
    struct shard_value patch;
    struct shard_value configure;
    struct shard_value build;
    struct shard_value test;
    struct shard_value install;

    struct {
        bool meta;
        bool derivation;
    } flags;
};

package_t *geode_load_package_file(struct geode_context *context, struct geode_package_index *index, const char *filepath);

struct shard_set *geode_call_package(struct geode_context *context, struct geode_package_index *index, struct shard_value value);
package_t *geode_decl_package(struct geode_context *context, struct geode_package_index *index, const char *origin, struct shard_set *decl);

#endif /* _SHARD_PACKAGE_H */

