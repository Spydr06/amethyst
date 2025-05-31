#include "defaults.h"
#include "exception.h"
#include "geode.h"
#include "git.h"
#include "include/store.h"
#include "lifetime.h"
#include "log.h"
#include "util.h"

#include "../shard_libc_driver.h"

#include <asm-generic/ioctls.h>
#include <assert.h>
#include <errno.h>
#include <memory.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

int geode_mkcontext(struct geode_context *context, const char *progname) {
    memset(context, 0, sizeof(struct geode_context));

    context->progname = progname;

    context->prefix_path.string = DEFAULT_PREFIX;
    context->store_path.string = DEFAULT_STORE_PATH;
    context->pkgs_path.string = DEFAULT_PKGS_PATH;
    context->config_path.string = DEFAULT_CONFIG_PATH;
    context->module_path.string = DEFAULT_MODULES_PATH;

    context->flags.out_no_color = isatty(fileno(stdout)) != 1;
    context->flags.err_no_color = isatty(fileno(stderr)) != 1;

    // init shard context
    int err = shard_context_default(&context->shard);
    if(err)
        geode_panic(context, "Failed creating shard context: %s", strerror(errno));

    context->shard.home_dir = NULL;
    context->shard.userp = context;

    err = shard_init(&context->shard);
    if(err)
        geode_panic(context, "Failed initializing shard context: %s\n", strerror(errno));

    context->initial_workdir = geode_getcwd(&context->l_global);
    assert(context->initial_workdir != NULL);
    assert(geode_pushd(context, context->initial_workdir) == 0);

    geode_store_init(context, &context->intrinsic_store);
    geode_store_init(context, &context->store);

    srand(time(NULL));

    return 0;
}

void geode_delcontext(struct geode_context *context) {
    geode_store_free(&context->store);
    geode_store_free(&context->intrinsic_store);

    geode_unlink_tmpfiles(context);

    git_shutdown(context);
    shard_deinit(&context->shard);
    l_free(&context->l_global);

    while(context->dirstack.count > 0)
        geode_popd(context);
    free(context->dirstack.dfds);
}

void geode_set_prefix(struct geode_context *context, char *prefix_path) {
    context->prefix_path.string = prefix_path;
    context->prefix_path.overwritten = true;

    if(!context->store_path.overwritten)
        context->store_path.string = l_strcat(&context->l_global, prefix_path, DEFAULT_STORE_PATH);
    if(!context->config_path.overwritten)
        context->config_path.string = l_strcat(&context->l_global, prefix_path, DEFAULT_CONFIG_PATH);
    if(!context->pkgs_path.overwritten)
        context->config_path.string = l_strcat(&context->l_global, prefix_path, DEFAULT_PKGS_PATH);
    if(!context->module_path.overwritten)
        context->module_path.string = l_strcat(&context->l_global, prefix_path, DEFAULT_MODULES_PATH);
}

void geode_set_module_dir(struct geode_context *context, char *module_path) {
    context->module_path.string = module_path;
    context->module_path.overwritten = true;
}

void geode_set_store(struct geode_context *context, char *store_path) {
    context->store_path.string = store_path;
    context->store_path.overwritten = true;
}

void geode_set_config(struct geode_context *context, char *config_path) {
    context->config_path.string = config_path;
    context->config_path.overwritten = true;
}

void geode_set_pkgs_dir(struct geode_context *context, char *pkgs_path) {
    context->pkgs_path.string = pkgs_path;
    context->pkgs_path.overwritten = true;
}

int geode_set_jobcnt(struct geode_context *context, char *jobcnt) {
    errno = 0;
    context->jobcnt = (int) strtol(jobcnt, NULL, 10);
    return errno;
}

void geode_set_verbose(struct geode_context *context, bool verbose) {
    context->flags.verbose = verbose;
}

void geode_update_termsize(struct geode_context *context) {
    if((context->outstream.isatty = isatty(STDOUT_FILENO))
            && ioctl(STDOUT_FILENO, TIOCGWINSZ, &context->outstream.termsize) == -1) 
        geode_throw(context, geode_io_ex(context, errno, "Could not get stdout terminal size"));

    if((context->errstream.isatty = isatty(STDERR_FILENO))
            && ioctl(STDERR_FILENO, TIOCGWINSZ, &context->errstream.termsize) == -1) 
        geode_throw(context, geode_io_ex(context, errno, "Could not get stderr terminal size"));
}

