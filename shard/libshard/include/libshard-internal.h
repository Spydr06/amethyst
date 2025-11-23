#ifndef _LIBSHARD_INTERNAL_H
#define _LIBSHARD_INTERNAL_H

#ifndef _LIBSHARD_INTERNAL
    #error "Never include <libshard-internal.h> outside of the libshard source code."
#endif /* _LIBSHARD_INTERNAL */

#include <assert.h>
#include <string.h>

#define __dynarr_append(arr, item, realloc, ...) do {                                           \
    if((arr)->count >= (arr)->capacity) {                                                       \
        (arr)->capacity = (arr)->capacity == 0 ? DYNARR_INIT_CAPACITY : (arr)->capacity * 2;    \
        (arr)->items = realloc(__VA_ARGS__ __VA_OPT__(,)                                        \
                (arr)->items, (arr)->capacity * sizeof(*(arr)->items));                         \
        assert((arr)->items != NULL);                                                           \
    }                                                                                           \
    (arr)->items[(arr)->count++] = (item);                                                      \
} while(0)

#define __dynarr_append_many(arr, new_items, new_items_count, realloc, ...) do {                   \
        if ((arr)->count + (new_items_count) > (arr)->capacity) {                                  \
            if ((arr)->capacity == 0) {                                                            \
                (arr)->capacity = DYNARR_INIT_CAPACITY;                                            \
            }                                                                                      \
            while ((arr)->count + (new_items_count) > (arr)->capacity) {                           \
                (arr)->capacity *= 2;                                                              \
            }                                                                                      \
            (arr)->items = realloc(__VA_ARGS__ __VA_OPT__(,)                                       \
                    (arr)->items, (arr)->capacity*sizeof(*(arr)->items));                          \
            assert((arr)->items != NULL && "Not enough memory.");                                  \
        }                                                                                          \
        memcpy((arr)->items + (arr)->count, (new_items), (new_items_count)*sizeof(*(arr)->items)); \
        (arr)->count += (new_items_count);                                                         \
    } while (0)

#define __dynarr_free(arr, free, ...) do {            \
    if((arr)->items) {                                \
        free(__VA_ARGS__ __VA_OPT__(,) (arr)->items); \
        (arr)->items = NULL;                          \
        (arr)->capacity = 0;                          \
    }                                                 \
} while(0)

#define dynarr_append(ctx, arr, item) __dynarr_append(arr, item, (ctx)->realloc)
#define dynarr_append_many(ctx, arr, new_items, new_items_count) __dynarr_append_many(arr, new_items, new_items_count, (ctx)->realloc)
#define dynarr_free(ctx, arr) __dynarr_free(arr, (ctx)->free)

#define dynarr_gc_append(gc, arr, item) __dynarr_append(arr, item, shard_gc_realloc, (gc))
#define dynarr_gc_append_many(gc, arr, new_items, new_items_count) __dynarr_append_many(arr, new_items, new_items_count, shard_gc_realloc, (gc))
#define dynarr_gc_free(gc, arr) __dynarr_free(arr, shard_gc_free, (gc))

#define EITHER(a, b) ((a) ? (a) : (b))
#define LEN(arr) (sizeof((arr)) / sizeof((arr)[0]))

#define MIN(a, b) ((a) < (b) ? (a) : (b))
#define MAX(a, b) ((a) > (b) ? (a) : (b))

#define NULL_VAL() ((struct shard_value) { .type = SHARD_VAL_NULL })
#define TRUE_VAL() BOOL_VAL(true)
#define FALSE_VAL() BOOL_VAL(false)

#define BOOL_VAL(b) ((struct shard_value) { .type = SHARD_VAL_BOOL, .boolean = (bool)(b) })
#define INT_VAL(i) ((struct shard_value) { .type = SHARD_VAL_INT, .integer = (int64_t)(i) })
#define FLOAT_VAL(f) ((struct shard_value) { .type = SHARD_VAL_FLOAT, .floating = (double)(f) })

#define STRING_VAL(s, l) ((struct shard_value) { .type = SHARD_VAL_STRING, .string = (s), .strlen = (l) })
#define CSTRING_VAL(s) STRING_VAL(s, strlen((s)))

#define PATH_VAL(p, l) ((struct shard_value) { .type = SHARD_VAL_PATH, .path = (p), .pathlen = (l) })
#define CPATH_VAL(p) PATH_VAL(p, strlen(p))

#define LIST_VAL(_head) ((struct shard_value) { .type = SHARD_VAL_LIST, .list.head = (_head) })
#define SET_VAL(_set) ((struct shard_value) { .type = SHARD_VAL_SET, .set = (_set) })
#define FUNC_VAL(_arg, _body, _scope) ((struct shard_value) { .type = SHARD_VAL_FUNCTION, .function.arg = (_arg), .function.body = (_body), .function.scope = (_scope) })

#define BUILTIN_VAL(_builtin) ((struct shard_value) {.type = SHARD_VAL_BUILTIN, .builtin.builtin=(_builtin), .builtin.queued_args=NULL, .builtin.num_queued_args=0 })
#ifndef _AMETHYST_STRNLEN_DEFINED

static inline size_t strnlen(const char *s, size_t n)
{
	const char *p = memchr(s, 0, n);
	return p ? (size_t) (p - s) : n;
}

#endif

static inline char* shard_mempcpy(void *restrict dst, const void *restrict src, size_t n) {
    return (char *)memcpy(dst, src, n) + n;
}

static inline char* shard_stpncpy(char *restrict dst, const char *restrict src, size_t dsize) {
     size_t  dlen = strnlen(src, dsize);
     return memset(shard_mempcpy(dst, src, dlen), 0, dsize - dlen);
}

#ifdef RUNTIME_DEBUG
    #include <stdio.h>
    #define DBG_PRINTF(...) printf(__VA_ARGS__)
#else
    #define DBG_PRINTF(...)
#endif

#endif /* _LIBSHARD_INTERNAL_H */

