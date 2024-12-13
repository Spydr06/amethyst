#ifndef _GEODE_MEM_UTIL_H
#define _GEODE_MEM_UTIL_H

#include <assert.h>
#include <string.h>

#ifndef DYNARR_INIT_CAP
#define DYNARR_INIT_CAP 256
#endif

#define dynarr_g(base) struct { base (*items); size_t count; size_t capacity; }

// Append an item to a dynamic array
#define dynarr_append(ctx, da, item)                                                     \
    do {                                                                                 \
        if ((da)->count >= (da)->capacity) {                                             \
            (da)->capacity = (da)->capacity == 0 ? DYNARR_INIT_CAP : (da)->capacity*2;   \
            (da)->items = geode_realloc((ctx), (da)->items, (da)->capacity*sizeof(*(da)->items)); \
            assert((da)->items != NULL && "Not enough memory");                          \
        }                                                                                \
                                                                                         \
        (da)->items[(da)->count++] = (item);                                             \
    } while (0)

#define dynarr_free(ctx, da) geode_free((ctx), (da).items)

#define __foreach_ctr __i_##__LINE__

#define dynarr_foreach(typ, iter, da) for(typ (*iter) = (da)->items; iter < ((da)->items + (da)->count); iter++)

typedef dynarr_g(int) dynarr_int_t;

typedef dynarr_g(const char*) dynarr_charptr_t;

#endif /* _GEODE_MEM_UTIL_H */

