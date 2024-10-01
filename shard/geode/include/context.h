#ifndef _GEODE_CONTEXT_H
#define _GEODE_CONTEXT_H

#include <libshard.h>

struct geode_error;
struct geode_context;

typedef __attribute__((noreturn)) void (*geode_error_handler_t)(struct geode_context*, struct geode_error);

enum geode_error_type {
    GEODE_ERR_LIBSHARD_INIT = 1,
    GEODE_ERR_SHARD,
    GEODE_ERR_UNRECOGNIZED_ARG
};

struct geode_error {
    enum geode_error_type type;
    union {
        struct shard_errors errs;
        int err_no;
        const char* arg;
    } payload;
};

struct geode_context {
    const char* prog_name;

    struct shard_context shard_ctx;
    bool shard_initialized;

    const char* main_config_path;

    geode_error_handler_t error_handler; 
};

int geode_context_init(struct geode_context* ctx, const char* prog_name, geode_error_handler_t error_handler);
void geode_context_free(struct geode_context* ctx);

void geode_print_shard_error(struct shard_error* err);

#define geode_throw(_ctx, _type, _payload) (geode_throw_err((_ctx), (struct geode_error){.type = GEODE_ERR_##_type, .payload={_payload}}))

_Noreturn void geode_throw_err(struct geode_context* ctx, struct geode_error err);

#endif /* _GEODE_CONTEXT_H */

