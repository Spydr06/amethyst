#include <sys/syscall.h>

#include <sys/fd.h>

#include <errno.h>

__syscall syscallret_t _sys_dup2(struct cpu_context* __unused, int old_fd, int new_fd) {
    syscallret_t ret = {
        .ret = -1,
        ._errno = 0
    };

    ret._errno = fd_duplicate(new_fd, old_fd);
    ret.ret = new_fd;
    return ret;
}

__syscall syscallret_t _sys_dup(struct cpu_context* ctx, int old_fd) {
    syscallret_t ret = {
        .ret = -1,
        ._errno = 0
    };

    int new_fd = fd_reserve();
    if(new_fd < 0) {
        ret._errno = ENOMEM;
        return ret;
    }

    ret = _sys_dup2(ctx, old_fd, new_fd);
    if(ret._errno)
        fd_vacate(new_fd);
    return ret;
}

_SYSCALL_REGISTER(SYS_dup, _sys_dup, "dup", "%d");
_SYSCALL_REGISTER(SYS_dup2, _sys_dup2, "dup2", "%d, %d");
