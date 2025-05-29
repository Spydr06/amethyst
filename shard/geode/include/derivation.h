#ifndef _SHARD_DERIVATION_H
#define _SHARD_DERIVATION_H

#include "geode.h"
#include "lifetime.h"
#include "exception.h"

#include <string.h>

struct geode_derivation;

struct geode_dependency {
    const char *name;

    bool has_min_constraint;
    bool has_max_constraint;
    const char *min_version;
    const char *max_version;
};

enum geode_builder_type {
    GEODE_BUILDER_NATIVE,
    GEODE_BUILDER_SHARD
};

struct geode_builder {
    union {
        struct shard_lazy_value *shard;
        struct {
            void (*native)(struct geode_context *, struct geode_derivation *, void *);
            void *userp;
        };
    } callback;
    enum geode_builder_type type;
};

static inline struct geode_builder geode_native_builder(void (*callback)(struct geode_context *, struct geode_derivation *, void*), void *userp) {
    return (struct geode_builder){ .callback.native = callback, .callback.userp = userp, .type = GEODE_BUILDER_NATIVE };
}

static inline struct geode_builder geode_shard_builder(struct shard_lazy_value *callback) {
    return (struct geode_builder){ .callback.shard = callback, .type = GEODE_BUILDER_SHARD };
}

struct geode_derivation {
    const char *name;
    const char *version;

    uint8_t hash[16];
    const char *prefix;

    struct geode_builder builder;
};

static inline int compare_versions(const char *va, const char *vb) {
    return strcmp(va, vb);
}

struct geode_derivation *geode_mkderivation(struct geode_context *context, const char *name, const char *version, struct geode_builder builder);

void geode_call_builder(struct geode_context *context, struct geode_derivation *deriv);

struct shard_value geode_builtin_derivation(volatile struct shard_evaluator *e, struct shard_builtin *builtin, struct shard_lazy_value** args);
struct shard_value geode_builtin_storeEntry(volatile struct shard_evaluator *e, struct shard_builtin *builtin, struct shard_lazy_value** args);
struct shard_value geode_builtin_intrinsicStore(volatile struct shard_evaluator *e, struct shard_builtin *builtin, struct shard_lazy_value** args);

#endif /* _SHARD_DERIVATION_H */

