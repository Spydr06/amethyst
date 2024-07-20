#ifndef _LIBSHARD_UTIL_H
#define _LIBSHARD_UTIL_H

#include <libshard.h>

enum shard_string_reader_flags : uint8_t {
    SHARD_STRING_READER_AUTOFREE = 1
};

struct shard_string_reader {
    const char* buf;
    size_t buf_size;
    size_t offset;
    void (*dtor)(void*);
};

void shard_string_reader_new(struct shard_context* ctx, struct shard_string_reader* dest, const char* buf, size_t buf_size, enum shard_string_reader_flags flags);
void shard_string_reader_free(struct shard_string_reader* reader);

int shard_string_source(struct shard_context* ctx, struct shard_source* dest, const char* origin, const char* buf, size_t buf_size, enum shard_string_reader_flags flags);

#endif /* _LIBSHARD_UTIL_H */

