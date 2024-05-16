#ifndef _AMETHYST_LIBK_DYNARRAY_H
#define _AMETHYST_LIBK_DYNARRAY_H

#include <stddef.h>
#include <stdint.h>

typedef struct dynarray {
    size_t size;
    size_t alloc;
    size_t elem_size;
    uint8_t* elems;
} dynarr_t;

void dynarr_init(struct dynarray* dynarr, size_t item_size, size_t init_size);
void dynarr_free(struct dynarray* dynarr);

size_t dynarr_push(struct dynarray* dynarr, void* value);
void* dynarr_getelem(struct dynarray* dynarr, size_t index);

#endif /* _AMETHYST_LIBK_DYNARRAY_H */

