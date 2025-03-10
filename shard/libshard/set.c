#define _LIBSHARD_INTERNAL
#include <libshard.h>

#include <errno.h>

struct shard_set* shard_set_init(struct shard_context* ctx, size_t capacity) {
    struct shard_set* set = shard_gc_malloc(ctx->gc, sizeof(struct shard_set) + sizeof(set->entries[0]) * capacity);
    if(!set) {
        errno = ENOMEM;
        return NULL;
    }
    
    set->capacity = capacity;
    set->size = 0;

    memset(set->entries, 0, sizeof(set->entries[0]) * capacity);

    return set;
}

struct shard_set* shard_set_from_hashmap(volatile struct shard_evaluator* e, struct shard_hashmap* map) {
    struct shard_set* set = shard_set_init(e->ctx, map->size);
    if(!set)
        return NULL;

    for(size_t i = 0; i < map->alloc; i++) {
        struct shard_hashpair* pair = map->pairs + i;
        if(pair->key)
            shard_set_put(set, pair->key, shard_lazy(e->ctx, pair->value, e->scope));
    }

    return set;
}

static inline uintptr_t hash(shard_ident_t attr) {
    return ((uintptr_t) attr) >> 3;
}

static inline uintptr_t probe_next(const struct shard_set* set, uintptr_t index) {
    return (index + 1) % set->capacity;
}

void shard_set_put(struct shard_set* set, shard_ident_t attr, struct shard_lazy_value* value) {
    if(!attr || set->size >= set->capacity)
        return;

    set->size++;

    uintptr_t index = hash(attr) % set->capacity;

    // linear probing
    for(size_t i = 0; i < set->capacity; i++) {
        if(!set->entries[index].key || set->entries[index].key == attr) {
            set->entries[index].key = attr;
            set->entries[index].value = value;
            return;
        }

        index = probe_next(set, index);
    }

    assert(false && "unreachable section reached in `shard_set_put()`");
    return;
}

int shard_set_get(struct shard_set* set, shard_ident_t attr, struct shard_lazy_value** value) {
    if(!attr)
        return EINVAL;

    if(!set->size)
        return ENOENT;

    uintptr_t index = hash(attr) % set->capacity;

    // linear probing
    for(size_t i = 0; i < set->capacity; i++) {
        if(set->entries[index].key == attr) {
            if(value)
                *value = set->entries[index].value;
            return 0;
        }

        index = probe_next(set, index);
    }

    if(value)
        *value = NULL;
    return ENOENT;
}

struct shard_set* shard_set_merge(struct shard_context* ctx, const struct shard_set* fst, const struct shard_set* snd) {
    struct shard_set* dest = shard_set_init(ctx, fst->size + snd->size);
    if(!dest)
        return NULL;

    size_t i;
    for(i = 0; i < fst->capacity; i++) {
        if(fst->entries[i].key)
            shard_set_put(dest, fst->entries[i].key, fst->entries[i].value);
    }

    for(i = 0; i < snd->capacity; i++) {
        if(snd->entries[i].key)
            shard_set_put(dest, snd->entries[i].key, snd->entries[i].value);
    }

    return dest;
}

