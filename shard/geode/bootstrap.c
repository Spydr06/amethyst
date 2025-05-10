#include "exception.h"
#include "geode.h"
#include "defaults.h"
#include "log.h"
#include "package.h"
#include "util.h"

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

static void bootstrap_pkgs_dir(struct geode_context *context) {
    char *pkgs_dest = l_strcat(&context->l_global, context->prefix_path.string, DEFAULT_PKGS_PATH);

    geode_verbosef(context, "copying package index... [%s -> %s]", context->pkgs_path.string, pkgs_dest);

    int err = copy_recursive(context->pkgs_path.string, pkgs_dest);
    if(err)
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

    bootstrap_pkgs_dir(context);
    bootstrap_configuration_file(context);

    create_store(context);
    bootstrap_monorepo(context);

    struct geode_package_index pkg_index;
    geode_mkindex(&pkg_index);
    geode_index_packages(context, &pkg_index);

    geode_install_package_from_file(context, &pkg_index, context->config_path.string);

    return EXIT_SUCCESS;
}

