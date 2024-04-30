#ifndef _AMETHYST_LIBK_HASHTABLE_H
#define _AMETHYST_LIBK_HASHTABLE_H

#include <stdint.h>
#include <stddef.h>

struct hashentry {
    struct hashentry* next;
    struct hashentry* prev;

    uintmax_t hash;
    size_t keysize;

    void* key;
    void* value;
};

typedef struct hashtable {
    size_t entry_count;
    size_t capacity;
    struct hashentry** entries;
} hashtable_t;

int hashtable_init(hashtable_t *table, size_t size);
int hashtable_set(hashtable_t *table, void *value, void *key, size_t keysize, bool allocate);
int hashtable_get(hashtable_t *table, void **value, void *key, size_t keysize);
int hashtable_remove(hashtable_t *table, void *key, size_t keysize);
int hashtable_destroy(hashtable_t *table);

#endif /* _AMETHYST_LIBK_HASHTABLE_H */

