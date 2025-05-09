#include "geode.h"
#include "package.h"
#include "log.h"
#include "util.h"
#include "defaults.h"

#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>

int geode_bootstrap(struct geode_context *context, int argc, char *argv[]) {
    const char *config_path = argc > 0 ? argv[0] : context->config_path.string;
    if(argc > 1) {
        geode_errorf(context, "Invalid argument `%s'\nUsage: %s boostrap [<configuration file>]", argv[1], context->progname);
        return EXIT_FAILURE;
    }

    char *pkgs_dest = l_strcat(&context->l_global, context->prefix_path.string, DEFAULT_PKGS_PATH);
    geode_verbosef(context, "copying package index... [%s -> %s]", context->pkgs_path.string, pkgs_dest);
    int err = copy_recursive(context->pkgs_path.string, pkgs_dest);
    if(err) {
        geode_throw(context, geode_io_ex(context, err, "Failed copying package index"));
    }

//    package_t *pkg = geode_load_package(context, config_path);

    return EXIT_SUCCESS;
}
