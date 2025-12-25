#include <sys/syscall.h>
#include <sys/fd.h>
#include <mem/user.h>
#include <cpu/cpu.h>

#include <errno.h>

__syscall syscallret_t _sys_read(struct cpu_context* ctx __unused, int fd, void* buffer, size_t size) {
    syscallret_t ret = {
        .ret = -1
    };

    if(!is_userspace_addr(buffer)) {
        ret._errno = EFAULT;
        return ret;
    }

    struct file* file = fd_get(fd);
    if(!file || (file->flags & FILE_READ) == 0) {
        ret._errno = EBADF;
        goto cleanup;
    }

    if(!size) {
        ret.ret = 0;
        ret._errno = 0;
        goto cleanup;
    }

    size_t bytes_read;
    ret._errno = vfs_read(file->vnode, buffer, size, file->offset, &bytes_read, file_to_vnode_flags(file->flags));

    if(ret._errno)
        goto cleanup;

    file->offset += bytes_read;
    ret.ret = bytes_read;

cleanup:
    if(file)
        fd_release(file);

    return ret;
}

_SYSCALL_REGISTER(SYS_read, _sys_read, "read", "%d, %p, %zu");
