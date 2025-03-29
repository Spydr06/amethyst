#include <sys/syscall.h>

#include <sys/fd.h>
#include <mem/user.h>

#include <errno.h>

__syscall syscallret_t _sys_getdents(struct cpu_context*, int fd, struct amethyst_dirent *dirp, size_t count) {
    syscallret_t ret = {
        .ret = -1
    };

    if(!is_userspace_addr(dirp)) {
        ret._errno = EFAULT;
        return ret;
    }

    struct file* file = fd_get(fd);
    if(!file) {
        ret._errno = EBADF;
        goto cleanup;
    }

    if(!count) {
        ret.ret = 0;
        ret._errno = 0;
        goto cleanup;
    }

    size_t dents_read;
	ret._errno = vfs_getdents(file->vnode, dirp, count, file->offset, &dents_read);

    if(ret._errno)
        goto cleanup;

    file->offset += dents_read;
    ret.ret = dents_read * sizeof(struct amethyst_dirent);

cleanup:
    if(file)
        fd_release(file);

    return ret;
}

