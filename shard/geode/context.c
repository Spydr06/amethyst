#include "exception.h"
#include "geode.h"
#include "defaults.h"
#include "lifetime.h"

#include "../shard_libc_driver.h"
#include "log.h"

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

    return 0;
}

void geode_delcontext(struct geode_context *context) {
    shard_deinit(&context->shard);
    l_free(&context->l_global);
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
}

void geode_set_store(struct geode_context *context, char *store_path) {
    context->store_path.string = store_path;
    context->store_path.overwritten = true;
}

void geode_set_config(struct geode_context *context, char *config_path) {
    context->config_path.string = config_path;
    context->config_path.overwritten = true;
}

int geode_set_jobcnt(struct geode_context *context, char *jobcnt) {
    errno = 0;
    context->jobcnt = (int) strtol(jobcnt, NULL, 10);
    return errno;
}

void geode_set_verbose(struct geode_context *context, bool verbose) {
    context->flags.verbose = verbose;
}

