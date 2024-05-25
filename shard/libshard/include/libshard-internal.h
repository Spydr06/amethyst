#ifndef _LIBSHARD_INTERNAL_H
#define _LIBSHARD_INTERNAL_H

#ifndef _LIBSHARD_INTERNAL
    #error "Never include <libshard-internal.h> outside of the libshard source code."
#endif /* _LIBSHARD_INTERNAL */

#include <assert.h>
#include <string.h>

#define dynarr_append(ctx, arr, item) do {                                                      \
    if((arr)->count >= (arr)->capacity) {                                                       \
        (arr)->capacity = (arr)->capacity == 0 ? DYNARR_INIT_CAPACITY : (arr)->capacity * 2;    \
        (arr)->items = (ctx)->realloc((arr)->items, (arr)->capacity * sizeof(*(arr)->items));   \
        assert((arr)->items != NULL);                                                           \
    }                                                                                           \
    (arr)->items[(arr)->count++] = (item);                                                      \
} while(0)

#define dynarr_append_many(ctx, arr, new_items, new_items_count) do {                           \
        if ((arr)->count + (new_items_count) > (arr)->capacity) {                                 \
            if ((arr)->capacity == 0) {                                                          \
                (arr)->capacity = DYNARR_INIT_CAPACITY;                                      \
            }                                                                                   \
            while ((arr)->count + (new_items_count) > (arr)->capacity) {                          \
                (arr)->capacity *= 2;                                                            \
            }                                                                                   \
            (arr)->items = (ctx)->realloc((arr)->items, (arr)->capacity*sizeof(*(arr)->items));     \
            assert((arr)->items != NULL && "Buy more RAM lol");                                 \
        }                                                                                       \
        memcpy((arr)->items + (arr)->count, (new_items), (new_items_count)*sizeof(*(arr)->items)); \
        (arr)->count += (new_items_count);                                                       \
    } while (0)

#define dynarr_free(ctx, arr) do {      \
    if((arr)->items) {                  \
        (ctx)->free((arr)->items);      \
        (arr)->items = NULL;            \
        (arr)->capacity = 0;            \
    }                                   \
} while(0)

#define EITHER(a, b) ((a) ? (a) : (b))
#define LEN(arr) (sizeof((arr)) / sizeof((arr)[0]))

static inline size_t strnlen(const char *s, size_t n)
{
	const char *p = memchr(s, 0, n);
	return p ? (size_t) (p - s) : n;
}

static inline char* shard_mempcpy(void *restrict dst, const void *restrict src, size_t n) {
    return (char *)memcpy(dst, src, n) + n;
}

static inline char* shard_stpncpy(char *restrict dst, const char *restrict src, size_t dsize) {
     size_t  dlen = strnlen(src, dsize);
     return memset(shard_mempcpy(dst, src, dlen), 0, dsize - dlen);
}

#endif /* _LIBSHARD_INTERNAL_H */

