#include "amethyst/syscall.h"

#include <sys/syscall.h>
#include <internal/syscall.h>

int init_module(const char *path, const char *const *args, int flags) {
    return syscall(SYS_init_module, path, args, flags);
}

int finit_module(int fd, const char *const *args, int flags) {
    return syscall(SYS_finit_module, fd, args, flags);
}

