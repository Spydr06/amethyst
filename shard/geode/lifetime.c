#include "lifetime.h"

#include <assert.h>
#include <memory.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

//
// Forked and adapted from https://github.com/tsoding/arena
//
// Copyright 2022 Alexey Kutepov <reximkut@gmail.com>
// Under the MIT License
//

#define LIFETIME_REGION_DEFAULT_CAPACITY 8192

typedef struct lifetime_region {
    struct lifetime_region *next;
    size_t count;
    size_t capacity;
    uintptr_t data[];
} region_t;

region_t *new_region(size_t capacity) {
    size_t size_bytes = sizeof(region_t) + sizeof(uintptr_t) * capacity;
    region_t *r = malloc(size_bytes);
    assert(r && "Not enough memory");

    r->next = NULL;
    r->count = 0;
    r->capacity = capacity;
    return r;
}

void free_region(region_t *r) {
    free(r);
}

void *l_malloc(lifetime_t *l, size_t size_bytes) {
    size_t size = (size_bytes + 2 * sizeof(uintptr_t) - 1)/sizeof(uintptr_t);

    if(l->end == NULL) {
        assert(l->begin == NULL);
        size_t capacity = LIFETIME_REGION_DEFAULT_CAPACITY;
        if (capacity < size) capacity = size;
        l->end = new_region(capacity);
        l->begin = l->end;
    }

    while(l->end->count + size > l->end->capacity && l->end->next != NULL) {
        l->end = l->end->next;
    }

    if(l->end->count + size > l->end->capacity) {
        assert(l->end->next == NULL);
        size_t capacity = LIFETIME_REGION_DEFAULT_CAPACITY;
        if (capacity < size) capacity = size;
        l->end->next = new_region(capacity);
        l->end = l->end->next;
    }

    uintptr_t *result = &l->end->data[l->end->count];
    result[0] = (uintptr_t) size;
    l->end->count += size;
    return (void*) (result + 1);
}

void *l_calloc(lifetime_t *l, size_t nmemb, size_t size) {
    void *ptr = l_malloc(l, nmemb * size);
    memset(ptr, 0, nmemb * size);
    return ptr;
}

void *l_realloc(lifetime_t *l, void *old_ptr, size_t new_size) {
    size_t old_size = (size_t) ((uintptr_t*) old_ptr)[-1];
    if(new_size <= old_size)
        return old_ptr;

    void *new_ptr = l_malloc(l, new_size);
    memcpy(new_ptr, old_ptr, old_size);
    return new_ptr;
}

char *l_strdup(lifetime_t *l, const char *str) {
    size_t len = strlen(str);
    char *dup = l_malloc(l, len + 1);
    memcpy(dup, str, len);
    dup[len] = '\0';
    return dup;
}

void *l_memdup(lifetime_t *l, const void *data, size_t size) {
    void *dup = l_malloc(l, size);
    memcpy(dup, data, size);
    return dup;
}

char *l_strcat(lifetime_t *l, const char *a, const char *b) {
    size_t len_a = strlen(a), len_b = strlen(b);
    size_t len = len_a + len_b;

    char *cat = l_malloc(l, len + 1);
    memcpy(cat, a, len_a);
    memcpy(cat + len_a, b, len_b);
    cat[len] = '\0';
    return cat;
}

char *l_sprintf(lifetime_t *l, const char *format, ...) {
    va_list ap;
    va_start(ap, format);
    char *str = l_vsprintf(l, format, ap);
    va_end(ap);
    return str;
}

char *l_vsprintf(lifetime_t *l, const char *format, va_list ap) {
    va_list ap_copy;
    va_copy(ap_copy, ap);
    int n = vsnprintf(NULL, 0, format, ap_copy);
    va_end(ap_copy);

    if(n < 0)
        return NULL;

    char *result = l_malloc(l, n + 1);
    vsnprintf(result, n + 1, format, ap);

    return result;
}

void l_reset(lifetime_t *l) {
    for(region_t *r = l->begin; r; r = r->next)
        r->count = 0;

    l->end = l->begin;
}

void l_free(lifetime_t *l) {
    region_t *r = l->begin;
    while(r) {
        region_t *r0 = r;
        r = r->next;
        free_region(r0);
    }
    l->begin = NULL;
    l->end = NULL;
}

void l_trim(lifetime_t *l) {
    if(!l->end)
        return;

    region_t *r = l->end->next;
    while(r) {
        region_t *r0 = r;
        r = r->next;
        free_region(r0);
    }
    l->end->next = NULL;
}

