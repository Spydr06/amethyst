#ifndef _SHARD_PACKAGE_H
#define _SHARD_PACKAGE_H

#include <libshard.h>

#include <stdint.h>

#include "mem_util.h"
#include "context.h"

typedef dynarr_g(struct package_spec) package_specs_t;

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

    struct shard_open_source* source;
};

struct package_index {
    dynarr_charptr_t spec_files;
    package_specs_t specs;
};

int geode_index_packages(struct geode_context* ctx, struct package_index* index);

int geode_compile_package_spec(struct geode_context* ctx, struct package_spec* spec, const char* filename);
int geode_validate_package_spec(struct geode_context* ctx, struct package_spec* spec);

int geode_load_package(struct geode_context* ctx, struct package_spec* spec, struct shard_lazy_value* src);

_Noreturn void geode_throw_package_error(struct geode_context* ctx, struct package_spec* spec, const char* fmt, ...);

void geode_print_package_spec_error(struct geode_context* ctx, struct package_spec* spec, const char* msg);

#endif /* _SHARD_PACKAGE_H */

