#include <sys/syscall.h>
#include <sys/fd.h>

__syscall syscallret_t _sys_close(struct cpu_context* __unused, int fd) {
    return (syscallret_t){
        ._errno = fd_close(fd),
        .ret = 0
    };
}
