#include "geode.h"
#include "package.h"

int geode_query(struct geode_context *context, int argc, char *argv[]) {

    struct geode_package_index index;
    geode_index_packages(context, &index);

    return EXIT_SUCCESS;
}
