#define _LIBSHARD_INTERNAL
#include <libshard.h>

void shard_string_append(struct shard_context* ctx, struct shard_string* str, const char* str2) {
    size_t len = strlen(str2);
    dynarr_append_many(ctx, str, str2, len);
}

SHARD_DECL void shard_string_appendn(struct shard_context* ctx, struct shard_string* str, const char* str2, size_t size) {
    dynarr_append_many(ctx, str, str2, size);
}

void shard_string_push(struct shard_context* ctx, struct shard_string* str, char c) {
    dynarr_append(ctx, str, c);
}

void shard_string_free(struct shard_context* ctx, struct shard_string* str) {
    dynarr_free(ctx, str);
}

SHARD_DECL void shard_gc_string_append(volatile struct shard_gc* gc, struct shard_string* str, const char* str2) {
    size_t len = strlen(str2);
    dynarr_gc_append_many(gc, str, str2, len);
}

SHARD_DECL void shard_gc_string_appendn(volatile struct shard_gc* gc, struct shard_string* str, const char* str2, size_t size) {
    dynarr_gc_append_many(gc, str, str2, size);
}

SHARD_DECL void shard_gc_string_push(volatile struct shard_gc* gc, struct shard_string* str, char c) {
    dynarr_gc_append(gc, str, c);
}

SHARD_DECL void shard_gc_string_free(volatile struct shard_gc* gc, struct shard_string* str) {
    dynarr_gc_free(gc, str);
}

SHARD_DECL size_t shard_list_length(struct shard_list* list) {
    size_t counter;
    for(counter = 0; list; counter++, list = list->next);
    return counter;
}
