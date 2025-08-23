#include "derivation.h"

#include "exception.h"
#include "geode.h"
#include "hash.h"
#include "include/log.h"
#include "include/store.h"
#include "libshard.h"
#include "util.h"

#include <libshard.h>

#include <memory.h>

#define NULL_VAL() ((struct shard_value) { .type = SHARD_VAL_NULL })

#define STRING_VAL(s, l) ((struct shard_value) { .type = SHARD_VAL_STRING, .string = (s), .strlen = (l) })
#define CSTRING_VAL(s) STRING_VAL(s, strlen((s)))

#define PATH_VAL(p, l) ((struct shard_value) { .type = SHARD_VAL_PATH, .path = (p), .pathlen = (l) })
#define CPATH_VAL(p) PATH_VAL(p, strlen(p))

void create_prefix(struct geode_context *context, struct geode_derivation *deriv) {
    if(!deriv->prefix) {
        uint64_t low_hash = ((uint64_t *) deriv->hash)[0];
        uint64_t high_hash = ((uint64_t *) deriv->hash)[1];

        deriv->prefix = l_sprintf(&context->l_global, "%s/%016lx%016lx-%s-%s", context->store_path.string, high_hash, low_hash, deriv->name, deriv->version);
    }

    int err;
    if(!fexists(deriv->prefix) && (err = mkdir_recursive(deriv->prefix, 0777)))
        geode_throw(context, geode_io_ex(context, err, "could not create derivation prefix `%s'", deriv->prefix));
}


struct geode_derivation *geode_mkderivation(struct geode_context *context, const char *name, const char *version, struct geode_builder builder) {
    struct geode_derivation *deriv = l_malloc(&context->l_global, sizeof(struct geode_derivation));

    memset(deriv, 0, sizeof(struct geode_derivation));

    deriv->name = name;
    deriv->version = version;
    deriv->builder = builder;

    size_t hash_data_len = strlen(deriv->name) + strlen(deriv->version) + 1;
    char *hash_data = malloc(hash_data_len + 1);
    assert(hash_data && "Not enough memory");

    snprintf(hash_data, hash_data_len + 1, "%s-%s", deriv->name, deriv->version);

    int err = geode_hash(GEODE_HASH_MD5, deriv->hash, (const uint8_t*) hash_data, hash_data_len);
    if(err) {
        free(hash_data);
        geode_throw(context, geode_io_ex(context, err, "Could not calculate hash for derivaiton `%s-%s'", deriv->name, deriv->version));
    }

    if((err = geode_store_register(&context->store, deriv)))
        geode_throwf(context, GEODE_EX_DERIV_DECL, "derivation `%s' is already defined", hash_data);

    create_prefix(context, deriv);

    geode_infof(context, "Using derivation `%s'", hash_data);

    free(hash_data);
    return deriv;
}

void geode_call_builder(struct geode_context *context, struct geode_derivation *deriv) {
    int err = geode_pushd(context, deriv->prefix);
    if(err)
        geode_throw(context, geode_io_ex(context, err, "Could not chdir into derivation prefix `%s'", deriv->prefix));

    switch(deriv->builder.type) {
    case GEODE_BUILDER_NATIVE:
        deriv->builder.callback.native(context, deriv, deriv->builder.callback.userp);
        break;
    case GEODE_BUILDER_SHARD: 
        if(shard_eval_lazy(&context->shard, deriv->builder.callback.shard))
            geode_throw(context, geode_shard_ex(context));
        break;
    default:
        assert(!"unreachable");
    }

    if((err = geode_popd(context)))
        geode_throw(context, geode_io_ex(context, err, "Could not chdir out of derivation prefix `%s'", deriv->prefix));
}

static const char *deriv_lockfile_path(struct geode_context *context, struct geode_derivation *deriv) {
    if(!deriv->lockfile)
        deriv->lockfile = l_sprintf(&context->l_global, "%s/derivation.lock", deriv->prefix);

    return deriv->lockfile;
}

bool geode_load_lockfile(struct geode_context *context, struct geode_derivation *deriv) {
    deriv_lockfile_path(context, deriv);

    if(!fexists(deriv->lockfile))
        return false;

    geode_panic(context, "%s exists", deriv->lockfile);
    return false;
}

void geode_save_lockfile(struct geode_context *context, struct geode_derivation *deriv) {
    deriv_lockfile_path(context, deriv);

    geode_headingf(context, "Saving lockfile `%s`...", deriv->lockfile);
}

static struct shard_lazy_value *deriv_lazy_attr(struct geode_context *context, struct shard_value *deriv, const char *attr) {
    assert(deriv->type == SHARD_VAL_SET);

    struct shard_lazy_value *value = NULL;
    if(shard_set_get(deriv->set, shard_get_ident(&context->shard, attr), &value) || !value)
        return NULL;

    return value;
}

static struct shard_value *deriv_get_attr(struct geode_context *context, struct shard_value *deriv, const char *attr) {
    struct shard_lazy_value *value = deriv_lazy_attr(context, deriv, attr);
    if(!value)
        return NULL;

    if(shard_eval_lazy(&context->shard, value))
        geode_throw(context, geode_shard_ex(context));

    assert(value->evaluated);
    return &value->eval;
}

static struct shard_value *deriv_required_attr(struct geode_context *context, struct shard_value *deriv, const char *attr, enum shard_value_type attr_type) {
    struct shard_value *value = deriv_get_attr(context, deriv, attr);
    if(!value)
        geode_throwf(context, GEODE_EX_DERIV_DECL, "derivation set does not have required attr `%s'", attr);

    if(!(value->type & attr_type))
        geode_throwf(context, GEODE_EX_DERIV_DECL,
                "derivation attr `%s' does not have expected type `%s', got `%s'", 
                attr,
                shard_value_type_to_string(&context->shard, attr_type),
                shard_value_type_to_string(&context->shard, value->type)
        );

    return value;
}

struct shard_value geode_builtin_derivation(volatile struct shard_evaluator *e, struct shard_builtin *builtin, struct shard_lazy_value** args) {
    struct geode_context *context = e->ctx->userp;

    struct shard_value deriv_decl = shard_builtin_eval_arg(e, builtin, args, 0);
    struct shard_value deriv_result = NULL_VAL();

    volatile exception_t *ex = geode_catch(context, GEODE_ANY_EXCEPTION);
    if(ex) {
        deriv_result = geode_exception_to_shard_value(e->ctx, ex);
        goto cleanup;
    }

    struct shard_value *name = deriv_required_attr(context, &deriv_decl, "name", SHARD_VAL_STRING);
    struct shard_value *version = deriv_required_attr(context, &deriv_decl, "version", SHARD_VAL_STRING);
    struct shard_lazy_value *builder = deriv_lazy_attr(context, &deriv_decl, "builder");

    struct geode_derivation *deriv = geode_mkderivation(context, name->string, version->string, geode_shard_builder(builder));
    if(geode_load_lockfile(context, deriv))
        goto cached_deriv;

    geode_call_builder(context, deriv);
    geode_save_lockfile(context, deriv);

cached_deriv:
    deriv_result = CPATH_VAL(deriv->prefix);
    
cleanup:
    geode_pop_exception_handler(context);
    return deriv_result;
}

struct shard_value geode_builtin_storeEntry(volatile struct shard_evaluator *e, struct shard_builtin *builtin, struct shard_lazy_value** args) {
    struct geode_context *context = e->ctx->userp;

    struct shard_value deriv_path = shard_builtin_eval_arg(e, builtin, args, 0);

    const struct geode_derivation *deriv;
    int err = geode_store_get_by_prefix(&context->store, deriv_path.path, &deriv);
    if(err)
        shard_eval_throw(e, e->error_scope->loc, "`%s': %s", deriv_path.path, strerror(err));

    struct shard_set *entry = shard_set_init(e->ctx, 2);

    shard_set_put(entry, shard_get_ident(e->ctx, "name"), shard_unlazy(e->ctx, CSTRING_VAL(deriv->name)));
    shard_set_put(entry, shard_get_ident(e->ctx, "version"), shard_unlazy(e->ctx, CSTRING_VAL(deriv->version)));

    return (struct shard_value){.type=SHARD_VAL_SET, .set=entry};
}

struct shard_value geode_builtin_intrinsicStore(volatile struct shard_evaluator *e, struct shard_builtin *, struct shard_lazy_value**) {
    struct geode_context *context = e->ctx->userp;
    return geode_store_to_shard_value(e->ctx, &context->intrinsic_store);
}

