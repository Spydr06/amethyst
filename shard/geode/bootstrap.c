#include "exception.h"
#include "geode.h"
#include "defaults.h"
#include "git2/errors.h"
#include "git2/repository.h"
#include "git2/reset.h"
#include "log.h"
#include "package.h"
#include "util.h"
#include "git.h"

#include <stdio.h>
#include <memory.h>
#include <limits.h>
#include <errno.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/stat.h>

static void test_prefix_writable(struct geode_context *context) {
    const char *prefix = context->prefix_path.string;

    if(!fexists(prefix))
        geode_throw(context, geode_io_ex(context, ENOENT, "prefix [%s] does not exist", prefix));

    if(!fwritable(prefix))
        geode_throw(context, geode_io_ex(context, EACCES, "prefix [%s] is not writable", prefix));
}

static void bootstrap_shard_modules(struct geode_context *context) {
    char *modules_dest = l_strcat(&context->l_global, context->prefix_path.string, DEFAULT_MODULES_PATH);

    int err;
    if(fexists(modules_dest) && (err = rmdir_recursive(modules_dest)))
        geode_throw(context, geode_io_ex(context, err, "Directory `%s' exists but could not be removed", modules_dest));

    geode_verbosef(context, "copying shard modules... [%s -> %s]", context->module_path.string, modules_dest);

    if((err = copy_recursive(context->module_path.string, modules_dest)))
        geode_throw(context, geode_io_ex(context, err, "Failed copying shard modules"));

    context->module_path.string = modules_dest;
    context->module_path.overwritten = true;
}

static void bootstrap_pkgs_dir(struct geode_context *context) {
    char *pkgs_dest = l_strcat(&context->l_global, context->prefix_path.string, DEFAULT_PKGS_PATH);

    int err;
    if(fexists(pkgs_dest) && (err = rmdir_recursive(pkgs_dest)))
        geode_throw(context, geode_io_ex(context, err, "Directory `%s' exists but could not be removed", pkgs_dest));

    geode_verbosef(context, "copying package index... [%s -> %s]", context->pkgs_path.string, pkgs_dest);

    if((err = copy_recursive(context->pkgs_path.string, pkgs_dest)))
        geode_throw(context, geode_io_ex(context, err, "Failed copying package index"));

    context->pkgs_path.string = pkgs_dest;
    context->pkgs_path.overwritten = true;
}

static void bootstrap_configuration_file(struct geode_context *context) {
    const char *config_src = context->config_path.string;
    char *config_dest = l_strcat(&context->l_global, context->prefix_path.string, DEFAULT_CONFIG_PATH);

    geode_verbosef(context, "copying configuration.shard...");

    int err = copy_file(config_src, config_dest);
    if(err)
        geode_throw(context, geode_io_ex(context, err, "Failed copying configuration.shard"));

    context->config_path.string = config_dest;
    context->config_path.overwritten = true;
}

static void create_store(struct geode_context *context) {
    if(context->store_path.overwritten)
        geode_throwf(context, GEODE_EX_SUBCOMMAND, "Option `-s'/`--store' is not supported with `bootstrap'");

    if(!fexists(context->store_path.string) && mkdir(context->store_path.string, 0777) < 0)
        geode_throw(context, geode_io_ex(context, errno, "Failed creating store [%s]", context->store_path.string));
}

static void bootstrap_monorepo(struct geode_context *context) {    
    git_lazy_init(context);
    exception_t *ex = NULL;

    char cwd[PATH_MAX + 1];
    if(!getcwd(cwd, sizeof(cwd)))
        geode_throw(context, geode_io_ex(context, errno, "Could not get working directory"));

    geode_infof(context, "Bootstrapping monorepo [%s -> %s]...", cwd, context->store_path.string);

    struct git_repository *monorepo = NULL;
    int err = git_repository_open_ext(&monorepo, cwd, GIT_REPOSITORY_OPEN_NO_SEARCH, NULL);
    if(err < 0)
        geode_throw(context, geode_git_ex(context, git_error_last(), "Could not open monorepo in `%s'", cwd));
    
    assert(monorepo);

    const char *version_filename = "./" VERSION_FILENAME;
    char *version_string = NULL;

    if((err = (int) read_whole_file(&context->l_global, version_filename, &version_string)) < 0) {
        ex = geode_io_ex(context, -err, "Could not open `%s'", version_filename);
        goto cleanup_repo;
    }

    version_string = strtrim(version_string);

    struct geode_version version;
    if((err = geode_parse_version(&version, version_string))) {
        ex = geode_io_ex(context, EINVAL, "Could not parse version file `%s' [%s]", version_filename, version_string);
        goto cleanup_repo;
    }

    struct git_reference *head_ref = NULL;
    if((err = git_repository_head(&head_ref, monorepo))) {
        ex = geode_git_ex(context, git_error_last(), "Could not get head of repository `%s'", cwd);
        goto cleanup_repo;
    }

    struct git_object *head_obj = NULL;
    if((err = git_reference_peel(&head_obj, head_ref, GIT_OBJECT_COMMIT))) {
        ex = geode_git_ex(context, git_error_last(), "Could not get commit from head");
        goto cleanup_ref;
    }

    struct git_commit *head = (struct git_commit*) head_obj;

    char head_hash[64];
    memset(head_hash, 0, sizeof(head_hash));
    
    const struct git_oid *head_id = git_commit_id(head);
    if((err = git_oid_nfmt(head_hash, sizeof(head_hash), head_id))) {
        ex = geode_git_ex(context, git_error_last(), "Could not get ID of last commit");
        goto cleanup_obj;
    }

    memcpy(version.commit, head_hash, sizeof(version.commit));

    const char *pkg_name = AMETHYST_PKG_NAME;
    char *pkg_dir = geode_package_directory(context, &context->l_global, pkg_name, &version);

    struct git_repository *cloned = NULL;
    if(fexists(l_strcat(&context->l_global, pkg_dir, "/.git"))) {
        if((err = git_repository_open_ext(&cloned, pkg_dir, GIT_REPOSITORY_OPEN_NO_SEARCH, 0))) {
            ex = geode_git_ex(context, git_error_last(), "Could not open repository `%s'", pkg_dir);
            goto cleanup_obj;
        }

        struct git_reference *cloned_head_ref = NULL;
        if((err = git_repository_head(&cloned_head_ref, cloned))) {
            ex = geode_git_ex(context, git_error_last(), "Could not get head of repository `%s'", pkg_dir);
            goto cleanup_obj;
        }

        struct git_object *cloned_head_obj = NULL;
        if((err = git_reference_peel(&cloned_head_obj, cloned_head_ref, GIT_OBJECT_ANY))) {
            ex = geode_git_ex(context, git_error_last(), "Could not get object from HEAD reference");
            goto cleanup_cloned_head;
        }

        if((err = git_reset(cloned, cloned_head_obj, GIT_RESET_HARD, NULL))) {
            ex = geode_git_ex(context, git_error_last(), "Could not hard-reset repository `%s'", pkg_dir);
            goto cleanup_cloned_obj;
        }

cleanup_cloned_obj:
        git_object_free(cloned_head_obj);
cleanup_cloned_head:
        git_reference_free(cloned_head_ref);
        if(ex)
            goto cleanup_obj;
    }
    else {
        if((err = git_clone(&cloned, cwd, pkg_dir, NULL))) {
            ex = geode_git_ex(context, git_error_last(), "Could not clone repository [%s -> %s]", cwd, pkg_dir);
            goto cleanup_obj;
        }
    }

    geode_verbosef(context, "Patching `%s'...", pkg_dir);

    struct git_diff *head_diff = NULL;
    if((err = git_diff_index_to_workdir(&head_diff, monorepo, NULL, NULL))) {
        ex = geode_git_ex(context, git_error_last(), "Could not get diff between index and working directory");
        goto cleanup_cloned;
    }

    if((err = git_apply(cloned, head_diff, GIT_APPLY_LOCATION_WORKDIR, NULL))) {
        ex = geode_git_ex(context, git_error_last(), "Could not apply diff to cloned repository `%s'", pkg_dir);
        goto cleanup_diff;
    }

cleanup_diff:
    git_diff_free(head_diff);
cleanup_cloned:
    git_repository_free(cloned);
cleanup_obj:
    git_object_free(head_obj);
cleanup_ref:
    git_reference_free(head_ref);
cleanup_repo:
    git_repository_free(monorepo);

    if(ex)
        geode_throw(context, ex);
}

int geode_bootstrap(struct geode_context *context, int argc, char *argv[]) {
    switch(argc) {
    case 0:
        break;
    case 1:
        context->config_path.string = argv[0];
        context->config_path.overwritten = true;
        break;
    default:
        geode_errorf(context, "Invalid argument `%s'\nUsage: %s boostrap [<configuration file>]", argv[1], context->progname);
        return EXIT_FAILURE;
    }

    geode_headingf(context, "Bootstrapping `%s' to `%s'.", context->config_path.string, context->prefix_path.string);

    test_prefix_writable(context);

    bootstrap_shard_modules(context);

    bootstrap_pkgs_dir(context);
    bootstrap_configuration_file(context);

    create_store(context);
    bootstrap_monorepo(context);

    // start up shard with the copied files
    geode_prelude(context);

    struct geode_package_index pkg_index;
    geode_mkindex(&pkg_index);
    geode_index_packages(context, &pkg_index);

    geode_install_package_from_file(context, &pkg_index, context->config_path.string);

    return EXIT_SUCCESS;
}

