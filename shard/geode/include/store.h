#ifndef GEODE_STORE_H
#define GEODE_STORE_H

#include "lifetime.h"

#include <stddef.h>
#include <stdint.h>

#include <libshard.h>

struct geode_derivation;
struct geode_context;

struct geode_store {
    lifetime_t l;

    size_t capacity;
    size_t count;
    const struct geode_derivation **derivs;
};

void geode_store_init(struct geode_context *context, struct geode_store *store);
void geode_store_free(struct geode_store *store);

int geode_store_register(struct geode_store *store, const struct geode_derivation *deriv);
int geode_store_get(struct geode_store *store, const uint8_t *hash, const struct geode_derivation **deriv);
int geode_store_get_by_prefix(struct geode_store *store, const char *path, const struct geode_derivation **deriv);

struct shard_value geode_store_to_shard_value(struct shard_context *ctx, struct geode_store *store);

#endif /* GEODE_STORE_H */

