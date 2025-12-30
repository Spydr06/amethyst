#ifndef _INITSYS_H
#define _INITSYS_H

#define _AMETHYST_SOURCE

#include <libshard.h>

#define DEFAULT_MODULE_LIST "/etc/modules.shard"

int load_kernel_modules(struct shard_context *ctx, const char *path);

#endif /* _INITSYS_H */

