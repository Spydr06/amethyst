#include <sys/syscall.h>

#include <sys/proc.h>
#include <sys/fd.h>
#include <mem/user.h>
#include <errno.h>

__syscall syscallret_t _sys_ioctl(struct cpu_context* __unused, int fd, unsigned long request, void* arg) {
    syscallret_t ret = {
        .ret = -1
    };

    if(!is_userspace_addr(arg)) {
        ret._errno = EFAULT;
        return ret;
    }

    struct file* file = fd_get(fd);
    if(!file) {
        ret._errno = EBADF;
        return ret;
    }

    int r = 0;

    vop_lock(file->vnode);
    ret._errno = vop_ioctl(file->vnode, request, arg, &r, &_cpu()->thread->proc->cred);
    vop_unlock(file->vnode);
    ret.ret = ret._errno ? -1 : r;

    fd_release(file);

    return ret;
}

