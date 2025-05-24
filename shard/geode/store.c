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
