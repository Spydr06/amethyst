#include "exception.h"
#include "geode.h"
#include "defaults.h"
#include "git2/errors.h"
#include "git2/repository.h"
#include "git2/reset.h"
#include "include/derivation.h"
#include "libshard.h"
#include "log.h"
#include "util.h"
#include "git.h"

#include <git2/apply.h>
#include <git2/deprecated.h>
#include <git2/diff.h>
#include <git2/refs.h>

#include <stdio.h>
#include <memory.h>
#include <limits.h>
#include <errno.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <libgen.h>

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

/*static int update_submodule(struct git_submodule *sm, const char *name, void *userp) {
    struct geode_context *context = (struct geode_context *) userp;
    int err;

    // Init submodule (does not clone)
    err = git_submodule_init(sm, 0);
    if (err < 0) return err;

    // Update submodule (clone if needed, checkout correct commit)
    git_submodule_update_options opts = GIT_SUBMODULE_UPDATE_OPTIONS_INIT;
    opts.checkout_opts.checkout_strategy = GIT_CHECKOUT_SAFE;
    err = git_submodule_update(sm, 1, &opts);

    if (err < 0) {
        const git_error *e = git_error_last();
        geode_errorf(context, "Failed to update submodule %s: %s\n", name, e->message);
    } else {
        geode_verbosef(context, "Updated submodule `%s'", name);
    }

    return err;
}*/

static void monorepo_builder(struct geode_context *context, struct geode_derivation *deriv, void *userp) {
    struct git_repository *source = (struct git_repository *) userp;

    git_lazy_init(context);
    exception_t *ex = NULL;

    int err;
    struct git_repository *cloned = NULL;
    if(fexists(DEFAULT_SOURCE_DIR "/.git")) {
        if((err = git_repository_open_ext(&cloned, DEFAULT_SOURCE_DIR, GIT_REPOSITORY_OPEN_NO_SEARCH, 0))) {
            ex = geode_git_ex(context, git_error_last(), "Could not open repository `%s/%s'", deriv->prefix, DEFAULT_SOURCE_DIR);
            goto cleanup_repo;
        }

        struct git_reference *cloned_head_ref = NULL;
        if((err = git_repository_head(&cloned_head_ref, cloned))) {
            ex = geode_git_ex(context, git_error_last(), "Could not get head of repository `%s/%s'", deriv->prefix, DEFAULT_SOURCE_DIR);
            goto cleanup_repo;
        }

        struct git_object *cloned_head_obj = NULL;
        if((err = git_reference_peel(&cloned_head_obj, cloned_head_ref, GIT_OBJECT_ANY))) {
            ex = geode_git_ex(context, git_error_last(), "Could not get object from HEAD reference");
            goto cleanup_cloned_head;
        }

        if((err = git_reset(cloned, cloned_head_obj, GIT_RESET_HARD, NULL))) {
            ex = geode_git_ex(context, git_error_last(), "Could not hard-reset repository `%s/%s'", deriv->prefix, DEFAULT_SOURCE_DIR);
            goto cleanup_cloned_obj;
        }

cleanup_cloned_obj:
        git_object_free(cloned_head_obj);
cleanup_cloned_head:
        git_reference_free(cloned_head_ref);
        if(ex)
            goto cleanup_repo;
    }
    else {
        assert(context->dirstack.count > 0);

        if((err = git_clone(&cloned, context->initial_workdir, DEFAULT_SOURCE_DIR, NULL))) {
            ex = geode_git_ex(context, git_error_last(), "Could not clone repository [%s -> %s/%s]", context->initial_workdir, deriv->prefix, DEFAULT_SOURCE_DIR);
            goto cleanup_repo;
        }
    }

    // Initialize submodules (parse .gitmodules)
/*    if((err = git_submodule_foreach(cloned, update_submodule, context))) {
        ex = geode_git_ex(context, git_error_last(), "Could not synchronize submodules");
        goto cleanup_cloned;
    } */

    geode_verbosef(context, "Patching `%s/%s'...", deriv->prefix, DEFAULT_SOURCE_DIR);

    git_diff_options diff_opts = GIT_DIFF_OPTIONS_INIT;
    diff_opts.flags = GIT_DIFF_INCLUDE_UNTRACKED | GIT_DIFF_RECURSE_UNTRACKED_DIRS | GIT_DIFF_SHOW_UNTRACKED_CONTENT;

    struct git_diff *workdir_diff = NULL;
    if((err = git_diff_index_to_workdir(&workdir_diff, source, NULL, &diff_opts))) {
        ex = geode_git_ex(context, git_error_last(), "Could not get diff between index and working directory");
        goto cleanup_cloned;
    }

    if((ex = git_diff_create_untracked(context, workdir_diff, DEFAULT_SOURCE_DIR)))
        goto cleanup_diff;

    git_apply_options apply_opts = GIT_APPLY_OPTIONS_INIT;
    apply_opts.flags = GIT_APPLY_LOCATION_WORKDIR;
    if((err = git_apply(cloned, workdir_diff, GIT_APPLY_LOCATION_WORKDIR, &apply_opts))) {
        ex = geode_git_ex(context, git_error_last(), "Could not apply diff to cloned repository `%s/%s'", deriv->prefix, DEFAULT_SOURCE_DIR);
        goto cleanup_diff;
    }

cleanup_diff:
    git_diff_free(workdir_diff);
cleanup_cloned:
    git_repository_free(cloned);
cleanup_repo:
    git_repository_free(source);

    if(ex)
        geode_throw(context, ex);
}

static void bootstrap_monorepo(struct geode_context *context) {    
    git_lazy_init(context);
    exception_t *ex = NULL;
    int err;

    char *cwd = geode_getcwd(&context->l_global);
    if(!cwd)
        geode_throw(context, geode_io_ex(context, errno, "Could not get working directory"));

    const char *version_filename = "./" VERSION_FILENAME;
    char *version_string = NULL;

    if((err = (int) read_whole_file(&context->l_global, version_filename, &version_string)) < 0)
        geode_throw(context, geode_io_ex(context, -err, "Could not open `%s'", version_filename));

    version_string = strtrim(version_string);

    struct git_repository *monorepo = NULL;
    if((err = git_repository_open_ext(&monorepo, cwd, GIT_REPOSITORY_OPEN_NO_SEARCH, NULL)) < 0)
        geode_throw(context, geode_git_ex(context, git_error_last(), "Could not open git repository in `%s'", cwd));
    
    assert(monorepo);

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
        goto cleanup_ref;
    }

    char *version = l_sprintf(&context->l_global, "%s-%.7s", version_string, head_hash);

    git_object_free(head_obj);
cleanup_ref:
    git_reference_free(head_ref);
cleanup_repo:
    if(ex) {
        git_repository_free(monorepo);
        geode_throw(context, ex);
    }

    struct geode_derivation *deriv = geode_mkderivation(context, AMETHYST_SOURCE_PKG_NAME, version, geode_native_builder(monorepo_builder, monorepo));
    if((err = geode_store_register(&context->intrinsic_store, deriv)))
        geode_throwf(context, GEODE_EX_DERIV_DECL, "Could not register derivation `%s-%s' in intrinsic store", deriv->name, deriv->version);
    geode_call_builder(context, deriv);
}

static int bootstrap_finish(struct geode_context *context, struct shard_value result) {
    int exit_code = EXIT_SUCCESS;

    struct shard_lazy_value *system_deriv_path_value;
    if(shard_set_get(result.set, shard_get_ident(&context->shard, "outPath"), &system_deriv_path_value)) {
        geode_errorf(context, "Bootstrap result does not have required `outPath' attr");
        exit_code = EXIT_FAILURE;
        goto cleanup;
    }

    if(shard_eval_lazy(&context->shard, system_deriv_path_value) > 0)
        geode_throw(context, geode_shard_ex(context));
   
    if(system_deriv_path_value->eval.type != SHARD_VAL_PATH) {
        geode_errorf(context, 
                "Bootstrap result `outPath' attr is required to be of type `path', got `%s'",
                shard_value_type_to_string(&context->shard, system_deriv_path_value->eval.type)
            );
        exit_code = EXIT_FAILURE;
        goto cleanup;
    }

    char *copy_cmd = l_sprintf(&context->l_global, "cp %s %s/*.iso .", context->flags.verbose ? "-v" : "", system_deriv_path_value->eval.path);
    int err = system(copy_cmd);
    if(err < 0)
        geode_throw(context, geode_io_ex(context, errno, "cmd failed: %s", copy_cmd));
    
    if(WEXITSTATUS(err) != 0) {
        geode_errorf(context, "cmd exited with code %d: %s", WEXITSTATUS(err), copy_cmd);
        exit_code = EXIT_FAILURE;
        goto cleanup;
    }

cleanup:
    return exit_code;
}

int geode_bootstrap(struct geode_context *context, int argc, char *argv[]) {
    int exit_code = EXIT_SUCCESS;

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

    geode_load_builtins(context, "bootstrap");

    test_prefix_writable(context);

    bootstrap_shard_modules(context);

    bootstrap_pkgs_dir(context);
    bootstrap_configuration_file(context);

    create_store(context);
    bootstrap_monorepo(context);

    struct shard_open_source *config_src = shard_open(&context->shard, context->config_path.string);
    
    if(shard_eval(&context->shard, config_src))
        geode_throw(context, geode_shard_ex(context));
    
    assert(config_src->evaluated);

    struct shard_value result;
    geode_call_with_prelude(context, &result, config_src->result);

    struct shard_string result_str = {0};
    if(shard_value_to_string(&context->shard, &result_str, &result, 4))
        geode_throw(context, geode_shard_ex(context));

    shard_string_push(&context->shard, &result_str, '\0');

    if(result.type != SHARD_VAL_SET) {
        geode_errorf(context, "Unexpected bootstrap result: %s", result_str.items);
        exit_code = EXIT_FAILURE;
        goto cleanup;
    } 
    else
        geode_verbosef(context, "Bootstrap result: %s", result_str.items);

    exit_code = bootstrap_finish(context, result);

cleanup:
    shard_string_free(&context->shard, &result_str);
    return exit_code;
}

