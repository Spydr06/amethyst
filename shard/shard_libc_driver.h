#ifndef _SHARD_LIBC_DRIVER_H
#define _SHARD_LIBC_DRIVER_H

#include <libshard.h>
#include <stdio.h>

void print_shard_error(FILE* stream, struct shard_error* error);

// shard_context initializer
int shard_context_default(struct shard_context* ctx);

// callback functions passed to libshard
int shard_open_callback(const char* path, struct shard_source* dest, const char* restrict mode);
int shard_close_callback(struct shard_source* self);
int shard_read_all_callback(struct shard_source* self, struct shard_string* dest);
void shard_buffer_dtor_callback(struct shard_string* buffer);

void shard_emit_errors(struct shard_context *ctx);

#endif /* _SHARD_LIBC_DRIVER_H */

