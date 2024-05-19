#include <libshard.h>

#include <assert.h>

int shard_init(struct shard_context* context) {
    assert(context->malloc && context->realloc && context->free);

    return 0;
}

void shard_deinit(struct shard_context* context) {

}

int shard_run(struct shard_context* context, struct shard_source* src) {
    return 0;
}

struct shard_error* shard_get_errors(struct shard_context* context) {
    return NULL;
}

void shard_print_error(struct shard_error* error, FILE* outp) {
    fprintf(outp, "error: %s\n", error->msg);
}

