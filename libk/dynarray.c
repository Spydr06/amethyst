#include <mem/heap.h>
#include <kernelio.h>
#include <dynarray.h>
#include <string.h>

#define DEFAULT_ALLOC 32

static uint8_t* alloc_elems(uint8_t* elems, size_t elem_size, size_t alloc, size_t old_alloc) {
    void* new_elems = kmalloc(elem_size * alloc);
    if(!new_elems)
        panic("Failed allocating %zu bytes for dynamic array.", alloc * elem_size);

    if(elems) {
        memcpy(new_elems, elems, old_alloc);
        kfree(elems);
    }

    return new_elems;
}

void dynarr_init(struct dynarray* dynarr, size_t elem_size, size_t init_alloc) {
    dynarr->size = 0;
    dynarr->elem_size = elem_size;

    dynarr->alloc = init_alloc ? init_alloc : DEFAULT_ALLOC;
    dynarr->elems = alloc_elems(nullptr, elem_size, dynarr->alloc, 0);
}

void dynarr_free(struct dynarray* dynarr) {
    dynarr->alloc = 0;
    kfree(dynarr->elems);
}

size_t dynarr_push(struct dynarray* dynarr, void* value) {
    if(dynarr->size >= dynarr->alloc) {
        dynarr->elems = alloc_elems(dynarr->elems, dynarr->elem_size, dynarr->alloc * 2, dynarr->alloc);
        dynarr->alloc *= 2;
    }

    memcpy(&dynarr->elems[dynarr->size * dynarr->elem_size], value, dynarr->elem_size);
    return dynarr->size++;
}

void* dynarr_getelem(struct dynarray* dynarr, size_t index) {
    if(index >= dynarr->elem_size)
        return nullptr;

    return &dynarr->elems[index * dynarr->elem_size];
}

