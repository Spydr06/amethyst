#include <sys/module.h>

#include <sys/syscall.h>
#include <internal/syscall.h>

int finit_module(int fd, const char *const *args, int flags) {
    return syscall(SYS_finit_module, fd, args, flags);
}

