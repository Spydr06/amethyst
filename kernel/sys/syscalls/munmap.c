#include <sys/syscall.h>

#include <errno.h>

__syscall syscallret_t _sys_munmap(struct cpu_context* __unused, void* addr, size_t len) {
    if(addr > USERSPACE_END || addr < USERSPACE_START)
        return (syscallret_t) {
            .ret = -1,
            ._errno = EFAULT
        };
    
    if(len == 0 || len % PAGE_SIZE > 0)
        return (syscallret_t) {
            .ret = -1,
            ._errno = EINVAL
        };

    vmm_unmap(addr, len, 0);

    return (syscallret_t){0, 0};
}
