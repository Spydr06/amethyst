#include "git.h"

#include "exception.h"
#include "log.h"
#include "util.h"

#include <git2/global.h>
#include <git2/checkout.h>
#include <git2/remote.h>
#include <git2/repository.h>

#include <stdio.h>
#include <libgen.h>
#include <memory.h>

#ifdef __GNUC__
#include <execinfo.h>
#endif

void git_lazy_init(struct geode_context *context) {
    if(context->flags.git_initialized)
        return;

    git_libgit2_init();
}

void git_shutdown(struct geode_context *context) {
    if(!context->flags.git_initialized)
        return;

    int err = git_libgit2_shutdown();
    if(err < 0) {
    }
}

exception_t *geode_git_ex(struct geode_context *context, const git_error *e, const char *format, ...) {
    va_list ap;
    va_start(ap);

    exception_t *ex = l_malloc(&context->l_global, sizeof(exception_t));
    memset(ex, 0, sizeof(exception_t));

    ex->type = GEODE_EX_GIT;
    ex->description = l_vsprintf(&context->l_global, format, ap);
    ex->payload.git.error = e;

#ifdef __GNUC__
    ex->stacktrace_size = backtrace(ex->stacktrace, MAX_STACK_LEVELS);
#endif

    return ex;
}

exception_t *git_diff_create_untracked(struct geode_context *context, struct git_diff *diff, const char *repo_path) {
    int err;
    size_t num_deltas = git_diff_num_deltas(diff);

    for(size_t i = 0; i < num_deltas; i++) {
        const git_diff_delta *delta = git_diff_get_delta(diff, i);

        const char *path = delta->new_file.path;
        if(delta->status != GIT_DELTA_UNTRACKED || fexists(path))
            continue;
        
        char *full_path = l_sprintf(&context->l_global, "%s/%s", repo_path, path);
        
        char *path_copy = strdup(full_path);
        char *dir = dirname(path_copy);
        if(!fexists(dir) && (err = mkdir_recursive(dir, 0777)))
            return geode_io_ex(context, err, "Could not make directory `%s'", dir);

        free(path_copy);

        FILE *f = fopen(full_path, "wb");
        if(!f)
            return geode_io_ex(context, err, "Could not create file `%s'", full_path);
        fclose(f);

        geode_verbosef(context, "Create untracked file `%s'", full_path);
    }

    return NULL;
}

struct shard_value builtin_git_checkoutBranch(volatile struct shard_evaluator* e, struct shard_builtin* builtin, struct shard_lazy_value** args) {
    struct geode_context *context = e->ctx->userp;
    git_lazy_init(context);

    struct shard_value branch_name = shard_builtin_eval_arg(e, builtin, args, 0);
    struct shard_value repo_path = shard_builtin_eval_arg(e, builtin, args, 1);

    geode_verbosef(context, "`%s': checkout branch `%s'", repo_path.path, branch_name.string);

    int err;
    exception_t *ex = NULL;

    struct git_repository *repo = NULL;
    if((err = git_repository_open_ext(&repo, repo_path.path, GIT_REPOSITORY_OPEN_NO_SEARCH, 0))) {
        ex = geode_git_ex(context, git_error_last(), "Could not open repository `%s'", repo_path.path);
        goto cleanup;
    }

    struct git_remote *remote = NULL;
    if((err = git_remote_lookup(&remote, repo, "origin"))) {
        ex = geode_git_ex(context, git_error_last(), "Could get remote origin of git repository `%s'", repo_path.path);
        goto cleanup;
    }


    assert(!"unimplemented!");

    /*struct git_strarray refspecs = {
        .strings = NULL,
        .count = 1
    };
    
    struct git_checkout_options opts = GIT_CHECKOUT_OPTIONS_INIT;
    opts.checkout_strategy = GIT_CHECKOUT_SAFE;

    if((err = git_checkout_tree(repo, tree, &opts))) {
        ex = geode_git_ex(context, git_error_last(), "Could not checkout git branch `%s' in repository `^s'", branch_name.string, repo_path.path);
        goto cleanup_tree;
    }*/

    git_remote_free(remote);
    git_repository_free(repo);
cleanup:
    if(ex)
        geode_throw(context, ex);
    return (struct shard_value){.type = SHARD_VAL_NULL};
}

struct shard_value builtin_git_pullRepo(volatile struct shard_evaluator* e, struct shard_builtin* builtin, struct shard_lazy_value** args) {
    struct geode_context *context = e->ctx->userp;
    git_lazy_init(context);

    struct shard_value repo_path = shard_builtin_eval_arg(e, builtin, args, 0);

    geode_verbosef(context, "`%s': git pull", repo_path.path);

    int err;
    exception_t *ex = NULL;

    struct git_repository *repo = NULL;
    if((err = git_repository_open_ext(&repo, repo_path.path, GIT_REPOSITORY_OPEN_NO_SEARCH, 0))) {
        ex = geode_git_ex(context, git_error_last(), "Could not open repository `%s'", repo_path.path);
        goto cleanup;
    }

    struct git_remote *remote = NULL;
    if((err = git_remote_lookup(&remote, repo, "origin"))) {
        ex = geode_git_ex(context, git_error_last(), "Could get remote origin of git repository `%s'", repo_path.path);
        goto cleanup;
    }

    if((err = git_remote_fetch(remote, NULL, NULL, "pull"))) {
        ex = geode_git_ex(context, git_error_last(), "Could not fetch updates from origin of git repository `%s'", repo_path.path);
        goto cleanup;
    } 

//cleanup_repo:
    git_repository_free(repo);
cleanup:
    if(ex)
        geode_throw(context, ex);
    return (struct shard_value){.type = SHARD_VAL_NULL};
}

struct shard_value builtin_git_cloneRepo(volatile struct shard_evaluator* e, struct shard_builtin* builtin, struct shard_lazy_value** args) {
    struct geode_context *context = e->ctx->userp;
    git_lazy_init(context);

    struct shard_value repo_url = shard_builtin_eval_arg(e, builtin, args, 0);
    struct shard_value repo_path = shard_builtin_eval_arg(e, builtin, args, 1);

    geode_verbosef(context, "`%s': git clone `%s'", repo_path.path, repo_url.string);

    int err;
    exception_t *ex = NULL;

    struct git_repository *repo;
    if((err = git_clone(&repo, repo_url.string, repo_path.path, NULL))) {
        ex = geode_git_ex(context, git_error_last(), "`%s': failed cloning git repository `%s'", repo_path.path, repo_url.string);
        goto cleanup;
    }

    git_repository_free(repo);
cleanup:
    if(ex)
        geode_throw(context, ex);
    return (struct shard_value){.type = SHARD_VAL_NULL};
}

