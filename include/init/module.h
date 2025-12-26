#ifndef _AMETHYST_INIT_MODULE_H
#define _AMETHYST_INIT_MODULE_H

#include <amethyst/module.h>
#include <filesystem/vfs.h>

#define KMODULE_MAX 128

void kmodule_init(void);

int kmodule_load(struct vnode *node, size_t argc, char **args, enum amethyst_module_flags flags);

#endif /* _AMETHYST_INIT_MODULE_H */

