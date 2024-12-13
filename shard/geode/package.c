#include "libshard.h"
#include <package.h>

#include <config.h>
#include <context.h>
#include <fs_util.h>
#include <mem_util.h>
#include <geode.h>

#include <stdio.h>
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
    }

    return err;
}

int geode_load_package(struct geode_context* ctx, struct package_spec* spec, struct shard_lazy_value* src) {
    return -1;    
}

