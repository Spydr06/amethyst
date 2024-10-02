#ifndef _GEODE_BOOTSTRAP_H
#define _GEODE_BOOTSTRAP_H

#include <context.h>

struct bootstrap_flags {
    const char* initrd_path;
};

int geode_bootstrap(struct geode_context* ctx, int argc, char** argv);

#endif /* _GEODE_BOOTSTRAP_H */

