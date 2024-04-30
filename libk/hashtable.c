#include <hashtable.h>

int hashtable_init(hashtable_t *table, size_t size) {
}

int hashtable_set(hashtable_t *table, void *value, void *key, size_t keysize, bool allocate);
int hashtable_get(hashtable_t *table, void **value, void *key, size_t keysize);
int hashtable_remove(hashtable_t *table, void *key, size_t keysize);
int hashtable_destroy(hashtable_t *table);


