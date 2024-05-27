#define _LIBSHARD_INTERNAL
#include <libshard.h>

void shard_string_append(struct shard_context* ctx, struct shard_string* str, const char* str2) {
    size_t len = strlen(str2);
    dynarr_append_many(ctx, str, str2, len);
}

void shard_string_push(struct shard_context* ctx, struct shard_string* str, char c) {
    dynarr_append(ctx, str, c);
}

void shard_string_free(struct shard_context* ctx, struct shard_string* str) {
    dynarr_free(ctx, str);
}

