#define _LIBSHARD_INTERNAL
#include <libshard.h>

#include <errno.h>

static inline size_t default_hash(const void* data, size_t size)
{
    const uint8_t* bytes = data;
    size_t hash = 0;

    for(size_t i = 0; i < size; i++) {
        hash += *bytes++;
        hash += hash << 10;
        hash ^= hash >> 6;
    }

    hash += hash << 3;
    hash ^= hash >> 11;
    hash += hash << 15;

    return hash;
}

static inline size_t size_mod(const struct shard_hashmap* map, size_t hash) {
    return hash & (map->alloc - 1);
}

static inline size_t probe_next(const struct shard_hashmap* map, size_t index) {
    return size_mod(map, index + 1);
}

static inline size_t calc_size(const struct shard_hashmap* map) {
    size_t map_size = map->size + (map->size / 3);
    return map_size < map->alloc 
        ? map->alloc
        : (size_t) (1 << ((sizeof(uint64_t) << 3) - __builtin_clzl(map_size - 1)));
}

static inline size_t calc_index(const struct shard_hashmap* map, const char* key) {
    size_t hash = default_hash(key, strlen(key));
    return size_mod(map, hash);
}

static inline struct shard_hashpair* find_pair(const struct shard_hashmap* map, const char* key, bool find_empty) {
    size_t index = calc_index(map, key);

    // linear probing
    for(size_t i = 0; i < map->alloc; i++) {
        struct shard_hashpair* pair = &map->pairs[index];
        if(!pair->key)
            return find_empty ? pair : NULL;
        
        if(strcmp(key, pair->key) == 0)
            return pair;

        index = probe_next(map, index);
    }

    return NULL;
}

static inline void rehash(const struct shard_context* ctx, struct shard_hashmap* map, size_t size) {
    if(!map || size < map->size)
        return;

    size_t old_alloc = map->alloc;
    struct shard_hashpair* old_pairs = map->pairs;

    map->alloc = size;
    map->pairs = ctx->malloc(size * sizeof(struct shard_hashpair));
    memset(map->pairs, 0, size * sizeof(struct shard_hashpair));

    for(struct shard_hashpair* pair = old_pairs; pair < &old_pairs[old_alloc]; pair++) {
        if(!pair->key)
            continue;

        struct shard_hashpair* new_pair = find_pair(map, pair->key, true);
        assert(new_pair != NULL);
        *new_pair = *pair;
    }

    ctx->free(old_pairs);
}

void shard_hashmap_init(const struct shard_context* ctx, struct shard_hashmap* map, size_t size) {
    size_t actual_size = 1 << ((sizeof(uint64_t) << 3) - __builtin_clzl(size - 1));

    map->size = 0;
    map->alloc = actual_size;
    map->pairs = ctx->malloc(actual_size * sizeof(struct shard_hashpair));
    
    memset(map->pairs, 0, actual_size * sizeof(struct shard_hashpair));
}

void shard_hashmap_free(const struct shard_context* ctx, struct shard_hashmap* map) {
    ctx->free(map->pairs);
    map->pairs = NULL;
    map->alloc = 0;
}

int shard_hashmap_put(const struct shard_context* ctx, struct shard_hashmap* map, const char* key, void* value) {
    if(!key || !map)
        return EINVAL;

    size_t map_size = calc_size(map);

    if(map_size > map->alloc) 
        rehash(ctx, map, map_size);

    struct shard_hashpair* pair = find_pair(map, key, true);
    if(!pair)
        return ENOMEM;

    if(pair->key)
        return EEXIST;

    pair->key = key;
    pair->value = value;
    map->size++;

    return 0;
}

void* shard_hashmap_get(const struct shard_hashmap* map, const char* key) {
    struct shard_hashpair* pair = find_pair(map, key, false);
    return pair ? pair->value : NULL;
}

