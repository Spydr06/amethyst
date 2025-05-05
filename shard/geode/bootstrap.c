#include "geode.h"

#include "log.h"

#include <stdlib.h>

int geode_bootstrap(struct geode_context *context, int argc, char *[]) {
    if(argc > 0) {
        geode_errorf(context, "Subcommand `bootstrap' does not expect any additional arguments");
        return EXIT_FAILURE;
    }

    

    return EXIT_SUCCESS;
}
