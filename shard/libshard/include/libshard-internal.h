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

#define BUILTIN_VAL(_callback, _num_expected_args) ((struct shard_value) {  \
        .type = SHARD_VAL_BUILTIN,                                          \
        .builtin.callback = (_callback),                                    \
        .builtin.queued_args = NULL,                                        \
        .builtin.num_queued_args = 0,                                       \
        .builtin.num_expected_args = (_num_expected_args)                   \
    })

#define LAZY_VAL(_lazy, _scope) ((struct shard_lazy_value){.lazy = (_lazy), .scope = (_scope), .evaluated = false})
#define UNLAZY_VAL(_eval) ((struct shard_lazy_value){.eval = (_eval), .evaluated = true})

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

