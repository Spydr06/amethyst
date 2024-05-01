#include <hashtable.h>

#include <mem/slab.h>
#include <mem/heap.h>

#include <errno.h>
#include <string.h>

#define FNV1PRIME  0x100000001b3ull
#define FNV1OFFSET 0xcbf29ce484222325ull

static struct scache* hash_entry_cache = nullptr;

// Reference:
// https://en.wikipedia.org/wiki/Fowler%E2%80%93Noll%E2%80%93Vo_hash_function
//
static inline uint64_t fnv1ahash(void *buffer, size_t size) {
	uint8_t *ptr = buffer;
	uint8_t *top = ptr + size;
	uint64_t h = FNV1OFFSET;

	while (ptr < top) {
		h ^= *ptr++;
		h *= FNV1PRIME;
	}

	return h;
}

static struct hashentry* get_entry(hashtable_t *table, void *key, size_t keysize, uintmax_t hash) {
	uintmax_t tableoffset = hash % table->capacity;

	struct hashentry* entry = table->entries[tableoffset];

	while (entry) {
		if (entry->keysize == keysize && entry->hash == hash && memcmp(entry->key, key, keysize) == 0)
			break;
		entry = entry->next;
	}

	return entry;
}

int hashtable_init(hashtable_t *table, size_t size) {
    if(!hash_entry_cache) {
        hash_entry_cache = slab_newcache(sizeof(struct hashentry), 0, nullptr, nullptr);
        if(!hash_entry_cache)
            return ENOMEM;
    }

    table->capacity = size;
    table->entries = kmalloc(size * sizeof(struct hashentry*));
    memset(table->entries, 0, sizeof(struct hashentry*));

    return table->entries ? 0 : ENOMEM;
}

int hashtable_set(hashtable_t *table, void *value, void *key, size_t keysize, bool allocate) {
    uintmax_t hash = fnv1ahash(key, keysize);

    struct hashentry* entry = get_entry(table, key, keysize, hash);

    if(entry) {
        entry->value = value;
        return 0;
    }
    else if(!allocate)
        return ENOENT;

    uintmax_t table_offset = hash % table->capacity;

    entry = slab_alloc(hash_entry_cache);
    if(!entry)
        return ENOMEM;

    entry->key = kmalloc(keysize);
    if(entry->key == nullptr) {
        slab_free(hash_entry_cache, entry);
        return ENOMEM;
    }

    memcpy(entry->key, key, keysize);
    entry->prev = nullptr;
    entry->next = table->entries[table_offset];
    if(entry->next)
        entry->next->prev = entry;

    entry->value = value;
    entry->keysize = keysize;
    entry->hash = hash;
    table->entries[table_offset] = entry;
    table->entry_count++;

    return 0;
}

int hashtable_get(hashtable_t *table, void **value, void *key, size_t keysize) {
    uintmax_t hash = fnv1ahash(key, keysize);

    struct hashentry* entry = get_entry(table, key, keysize, hash);

    if(!entry)
        return ENOENT;

    *value = entry->value;
    return 0;
}

int hashtable_remove(hashtable_t *table, void *key, size_t keysize) {
    uintmax_t hash = fnv1ahash(key, keysize);
    uintmax_t table_offset = hash % table->capacity;

    struct hashentry* entry = get_entry(table, key, keysize, hash);

    if(!entry)
        return ENOENT;

    if(entry->prev)
        entry->prev->next = entry->next;
    else
        table->entries[table_offset] = entry->next;

    if(entry->next)
        entry->next->prev = entry->prev;

    kfree(entry->key);
    slab_free(hash_entry_cache, entry);
    table->entry_count--;

    return 0;
}

int hashtable_destroy(hashtable_t *table) {
    struct hashentry saved_entry;
    HASHTABLE_FOREACH(table, entry) {
        saved_entry = *entry;
        kfree(entry->key);
        slab_free(hash_entry_cache, entry);
        entry = &saved_entry;
    }

    return 0;
}


