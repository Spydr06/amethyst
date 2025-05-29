#include "store.h"

#include "lifetime.h"
#include "derivation.h"

#include <errno.h>

#define MAX(a, b) ((a) > (b) ? (a) : (b))

void geode_store_init(struct geode_context *, struct geode_store *store) {
    memset(store, 0, sizeof(struct geode_store));
}

void geode_store_free(struct geode_store *store) {
    free(store->derivs); 
    l_free(&store->l);
}

int geode_store_register(struct geode_store *store, const struct geode_derivation *deriv) {
    if(store->count + 1 > store->capacity) {
        size_t new_capacity = MAX(256, store->capacity * 2);
        void *new_buffer = realloc(store->derivs, new_capacity * sizeof(void*));
        if(!new_buffer)
            return errno;

        store->capacity = new_capacity;
        store->derivs = new_buffer;
    }

    store->derivs[store->count++] = deriv;
    return 0;
}

int geode_store_get(struct geode_store *store, const uint8_t *hash, const struct geode_derivation **deriv) {
    for(size_t i = 0; i < store->count; i++) {
        const struct geode_derivation *current = store->derivs[i];
        if(memcmp(hash, current->hash, sizeof(current->hash)) == 0) {
            *deriv = current;
            return 0;
        }
    }

    *deriv = NULL;
    return ENOENT;
}

int geode_store_get_by_prefix(struct geode_store *store, const char *path, const struct geode_derivation **deriv) {
    for(size_t i = 0; i < store->count; i++) {
        const struct geode_derivation *current = store->derivs[i];
        if(strcmp(path, current->prefix) == 0) {
            *deriv = current;
            return 0;
        }
    }

    *deriv = NULL;
    return ENOENT;
}

struct shard_value geode_store_to_shard_value(struct shard_context *ctx, struct geode_store *store) {
    struct shard_set *set = shard_set_init(ctx, store->count);
    
    for(size_t i = 0; i < store->count; i++) {
        const struct geode_derivation *current = store->derivs[i];

        shard_ident_t name = shard_get_ident(ctx, current->name);
        struct shard_lazy_value *prefix = shard_unlazy(ctx, (struct shard_value){
            .type=SHARD_VAL_STRING,
            .string=current->prefix,
            .strlen=strlen(current->prefix)
        });

        shard_set_put(set, name, prefix);
    }

    return (struct shard_value){.type=SHARD_VAL_SET, .set=set};
}

