#include "package.h"

#include "hash.h"

#include <errno.h>
#include <memory.h>

int geode_parse_version(struct geode_version *version, const char *str) {
    memset(version, 0, sizeof(struct geode_version));
    int v[4];

    char *next = (char*) str;
    for(size_t i = 0; i < __len(v); i++, next++) {
        v[i] = strtol(next, &next, 36);
        if(next[0] != '.' && next[0] == '-')
            break;
    }

    version->x = v[0];
    version->y = v[1];
    version->z = v[2];
    version->w = v[3];

  //  if(next[0] == '-' || next[0] == '.')
//        version->commit = strtol(next, &next, 16);

    return 0;
}

static const char base36_chars[] = "0123456789abcdefghijklmnopqrstuvwxyz";

char *geode_version_to_string(lifetime_t *l, struct geode_version *version) {
    
}

package_t *geode_load_package_file(struct geode_context *context, struct geode_package_index *index, const char *filepath) {
    struct shard_open_source* source = shard_open(&context->shard, filepath);
    if(!source)
        geode_throw(context, geode_io_ex(context, errno, "`%s'", filepath));
    
    int num_errors = shard_eval(&context->shard, source);
    if(num_errors > 0)
        geode_throw(context, geode_shard_ex(context));

    assert(source->evaluated);

    struct shard_value package_call = source->result;
    struct shard_set *package_decl = geode_call_package(context, index, package_call);
    return geode_decl_package(context, index, filepath, package_decl);
}

struct shard_set *geode_call_package(struct geode_context *context, struct geode_package_index *index, struct shard_value value) {
}

package_t *geode_decl_package(struct geode_context *context, struct geode_package_index *index, const char *origin, struct shard_set *decl) {

}

void geode_install_package_from_file(struct geode_context *context, struct geode_package_index *index, const char *filepath) {
    package_t *package = geode_load_package_file(context, index, filepath);
}

char *geode_package_directory(struct geode_context *context, lifetime_t *l, const char *pkg_name, struct geode_version *pkg_version) {
    uint8_t pkg_hash[16];
    geode_hash_pkg(pkg_hash, pkg_name, pkg_version);

    return l_sprintf(l, "%s/%016lx%016lx-%s", context->store_path.string, *(uint64_t*)pkg_hash, *((uint64_t*)pkg_hash + 1), pkg_name); 
}

void geode_hash_pkg(uint8_t hash[16], const char *pkg_name, struct geode_version *pkg_version) {
    struct {
        const char *name;
        struct geode_version version;
    } __attribute__((packed)) hash_data = { .name = pkg_name, .version = *pkg_version };

    geode_hash(GEODE_HASH_MD5, hash, (const uint8_t*) &hash_data, sizeof(hash_data));
}

