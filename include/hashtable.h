#ifndef _AMETHYST_LIBK_HASHTABLE_H
#define _AMETHYST_LIBK_HASHTABLE_H

#include <stdint.h>
#include <stddef.h>

#define HASHTABLE_FOREACH(table, entry)                                                             \
	for (uintmax_t toffset = 0; toffset < (table)->capacity; ++toffset)                             \
		for (struct hashentry* entry = (table)->entries[toffset]; entry != NULL; entry = entry->next)

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
int hashtable_set(hashtable_t *table, void *value, const void *key, size_t keysize, bool allocate);
int hashtable_get(hashtable_t *table, void **value, const void *key, size_t keysize);
int hashtable_remove(hashtable_t *table, void *key, size_t keysize);
int hashtable_destroy(hashtable_t *table);

size_t hashtable_size(hashtable_t* table);

#endif /* _AMETHYST_LIBK_HASHTABLE_H */

