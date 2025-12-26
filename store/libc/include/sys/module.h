#ifndef _SYS_MODULE_H
#define _SYS_MODULE_H

#include <amethyst/module.h>

int finit_module(int fd, const char *const *args, int flags);

#endif /* _SYS_MODULE_H */

