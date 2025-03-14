#define _LIBSHARD_INTERNAL
#include <libshard.h>
#include <libshard-util.h>

#include <stdio.h>

void shard_string_reader_new(struct shard_context* ctx, struct shard_string_reader* dest, const char* buf, size_t buf_size, enum shard_string_reader_flags flags) {
    dest->buf = buf;
    dest->buf_size = buf_size;
    dest->offset = 0;
    dest->dtor = NULL;

    if(flags & SHARD_STRING_READER_AUTOFREE)
        dest->dtor = ctx->free;
}

void shard_string_reader_free(struct shard_string_reader* reader) {
    if(reader->dtor)
        reader->dtor((void*) reader->buf);
}

static int read_all(struct shard_source* self, struct shard_string* dest) {
    struct shard_string_reader* reader = self->userp;
    dest->items = (char*) reader->buf;
    dest->count = reader->buf_size;
    dest->capacity = reader->buf_size;
    return 0;
}

static int _close(struct shard_source* src) {
    struct shard_string_reader* reader = src->userp;
    shard_string_reader_free(reader);
    return 0;
}

int shard_string_source(struct shard_context* ctx, struct shard_source* dest, const char* origin, const char* buf, size_t buf_size, enum shard_string_reader_flags flags) {
    struct shard_string_reader* reader = ctx->malloc(sizeof(struct shard_string_reader));
    shard_string_reader_new(ctx, reader, buf, buf_size, flags);

    *dest = (struct shard_source) {
        .userp = reader,
        .origin = origin,
        .close = _close,
        .line_offset = 1,
        .read_all = read_all,
        .buffer_dtor = NULL
    };

    return 0;
}

