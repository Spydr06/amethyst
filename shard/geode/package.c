#include "package.h"

#include <errno.h>

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
