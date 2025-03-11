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

static int _getc(struct shard_source* src) {
    struct shard_string_reader* reader = src->userp;
    
    if(reader->offset >= reader->buf_size)
        return EOF;

    return reader->buf[reader->offset++];
}

static int _ungetc(int ch, struct shard_source* src) {
    struct shard_string_reader* reader = src->userp;

    if(reader->offset == 0 || reader->offset > reader->buf_size)
        return EOF;

    reader->offset--;
    return ch;
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
        .getc = _getc,
        .ungetc = _ungetc,
        .close = _close,
    };

    return 0;
}

