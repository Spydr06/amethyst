#ifndef _LIBSHARD_H
#define _LIBSHARD_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>
#include <stdio.h>

struct shard_context {
    void* (*malloc)(size_t size);
    void* (*realloc)(void* ptr, size_t new_size);
    void (*free)(void* ptr);
};

int shard_init(struct shard_context* context);
void shard_deinit(struct shard_context* context);

// TODO: add other sources like memory buffers
struct shard_source {
    enum {
        SHARD_FILE,
    } type;

    union {
        FILE* file;
    };
};

int shard_run(struct shard_context* context, struct shard_source* src);

struct shard_error {
    const char* msg;
};

struct shard_error* shard_get_errors(struct shard_context* context);

void shard_print_error(struct shard_error* error, FILE* outp);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* _LIBSHARD_H */

