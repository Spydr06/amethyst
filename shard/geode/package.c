#include "libshard.h"
#include <package.h>

#include <config.h>
#include <context.h>
#include <fs_util.h>
#include <mem_util.h>
#include <geode.h>

#include <stdio.h>
#include <stdarg.h>
#include <errno.h>

#define __USE_MISC
#include <dirent.h>

int scan_pkg_specs(struct geode_context* ctx, dynarr_charptr_t* paths, const char* spec_path) {
    int err = 0;
    DIR* spec_dir = opendir(spec_path);
    if(!spec_dir)
        return errno;

    struct dirent* ent;
    while((ent = readdir(spec_dir))) {
        if(strcmp(ent->d_name, ".") == 0 || strcmp(ent->d_name, "..") == 0)
            continue;

        if(ent->d_type == DT_DIR) {
            char* subdir_path;
            if((err = geode_concat_paths(ctx, &subdir_path, NULL, (const char*[]){
                spec_path,
                ent->d_name,
                NULL
            })))
                goto finish;

            err = scan_pkg_specs(ctx, paths, spec_path);

            geode_free(ctx, subdir_path);
            goto finish;
        }
        else if(filename_has_file_ext(ent->d_name, "shard")) {
            char* spec_filename;
            if((err = geode_concat_paths(ctx, &spec_filename, NULL, (const char*[]){
                spec_path,
                ent->d_name,
                NULL
            })))
                goto finish;

            dynarr_append(ctx, paths, spec_filename);
        }
    }

    if(errno)
        err = errno;

finish:
    closedir(spec_dir);

    return err;
}

int geode_index_packages(struct geode_context* ctx, struct package_index* index) {
    if(!geode_config_loaded(ctx) || !index)
        return EINVAL;

    memset(index, 0, sizeof(struct package_index));

    int err;
    char* spec_path;
    
    err = geode_concat_paths(ctx, &spec_path, NULL, (const char*[]){
        ctx->store_path,
        GEODE_PKGSPEC_DIR,
        NULL
    });

    if(err)
        return err;

    if(ctx->flags.verbose)
        infof(ctx, "Loading package index from `%s`...\n", spec_path);

    dynarr_charptr_t paths = {0};
    
    err = scan_pkg_specs(ctx, &paths, spec_path);
    if(err)
        return err;

    dynarr_foreach(const char*, path, &paths) {
        if(ctx->flags.verbose)
            printf("  - %s\n", *path);

        struct package_spec spec;
        if((err = geode_compile_package_spec(ctx, &spec, *path)))
            return err;

        dynarr_append(ctx, &index->specs, spec);
    }

    return err;
}

int geode_compile_package_spec(struct geode_context* ctx, struct package_spec* spec, const char* filename) {
    if(!geode_config_loaded(ctx) || !spec)
        return EINVAL;

    memset(spec, 0, sizeof(struct package_spec));

    spec->source = shard_open(&ctx->shard_ctx, filename);

    int err = shard_eval(&ctx->shard_ctx, spec->source);
    if(err)
        geode_throw(ctx, SHARD, .shard=TUPLE(.num=err, .errs=shard_get_errors(&ctx->shard_ctx)));

    return geode_validate_package_spec(ctx, spec);
}

struct shard_lazy_value* package_spec_expect_attr(struct geode_context* ctx, struct package_spec* spec, const char* attr) {
    struct shard_lazy_value* lazy;
    int err;

    if((err = shard_set_get(spec->source->result.set, shard_get_ident(&ctx->shard_ctx, attr), &lazy)))
        geode_throw_package_error(ctx, spec, C_BLD "missing" C_RST " expected " C_BLD "attribute `%s`" C_RST ": %s", attr, strerror(err));

    return lazy;
}

void package_spec_expect_type(struct geode_context* ctx, struct package_spec* spec, const char* attr, struct shard_value* value, enum shard_value_type type) {
    if(value->type & type)
        return;

    char buf1[128], buf2[128];
    geode_throw_package_error(ctx, spec, C_BLD "attribute `%s`" C_RST " does not match expected " C_BLD "type `%s`" C_RST ", got `%s`",
            attr,
            shard_value_type_to_str(type, buf1, 128),
            shard_value_type_to_str(value->type, buf2, 128)
    );
}

struct shard_value package_spec_expect_typed_attr(struct geode_context* ctx, struct package_spec* spec, const char* attr, enum shard_value_type expected_type) {
    struct shard_lazy_value* lazy = package_spec_expect_attr(ctx, spec, attr);
    int err = shard_eval_lazy(&ctx->shard_ctx, lazy);
    if(err)
        geode_throw(ctx, SHARD, .shard=TUPLE(.num=err, .errs=shard_get_errors(&ctx->shard_ctx)));

    package_spec_expect_type(ctx, spec, "name", &lazy->eval, expected_type);

    return lazy->eval;
}

struct shard_lazy_value* package_spec_get_attr(struct geode_context* ctx, struct package_spec* spec, const char* attr) {
    struct shard_lazy_value* lazy;

    return (shard_set_get(spec->source->result.set, shard_get_ident(&ctx->shard_ctx, attr), &lazy)) ? NULL : lazy;    
}

int geode_validate_package_spec(struct geode_context* ctx, struct package_spec* spec) {
    if(!spec->source || !spec->source->evaluated)
        return EINVAL;

    struct shard_value spec_val = spec->source->result;

    if(spec_val.type != SHARD_VAL_SET)
        geode_throw_package_error(ctx, spec, "specification does not evaluate to type `set`");

    struct shard_value name = package_spec_expect_typed_attr(ctx, spec, "name", SHARD_VAL_STRING);

    printf("pkg: \"%s\"\n", name.string);

    return 0;
}

int geode_load_package(struct geode_context* ctx, struct package_spec* spec, struct shard_lazy_value* src) {
    return -1;    
}

_Noreturn void geode_throw_package_error(struct geode_context* ctx, struct package_spec* spec, const char* fmt, ...) {
    va_list ap, ap_copy;
    va_start(ap);
    va_copy(ap_copy, ap);

    int size = vsnprintf(NULL, 0, fmt, ap_copy);
    assert(size >= 0);

    char* msg = geode_malloc(ctx, (size + 1) * sizeof(char));
    vsnprintf(msg, size + 1, fmt, ap);

    va_end(ap);

    geode_throw(ctx, PACKAGE_SPEC, .pkgspec=TUPLE(.spec = spec, .msg = msg));
}

void geode_print_package_spec_error(struct geode_context* ctx, struct package_spec* spec, const char* msg) {
    (void) ctx;
    fprintf(stderr, C_RED C_BLD "error:" C_RST " package specification error");

    if(spec->name)
        fprintf(stderr, " in " C_BLD "package " SPECIAL_QUOTE("%s") C_RST, spec->name);

    fprintf(stderr, ":\n        " C_BLUE C_BLD "in " C_PURPLE "%s" C_RST "\n", spec->source->source.origin);

    fprintf(stderr, C_BLD C_BLACK "  - | " C_RST "%s.\n", msg);
    fprintf(stderr, C_BLD C_BLACK "    |" C_RST "\n");
}
