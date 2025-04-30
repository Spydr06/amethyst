#include <sys/syscall.h>

#include <sys/fd.h>
#include <filesystem/virtual.h>

#include <abi.h>
#include <errno.h>
#include <kernelio.h>

__syscall syscallret_t _sys_lseek(struct cpu_context*, int fd, off_t offset, unsigned origin) {
    syscallret_t ret = {
        .ret = -1
    };

    struct file* file = fd_get(fd);
    if(!file) {
        ret._errno = EBADF;
        goto cleanup;
    }

    if(file->vnode->type == V_TYPE_FIFO || file->vnode->type == V_TYPE_SOCKET /* || TODO: PIPE */) {
        ret._errno = ESPIPE;
        goto cleanup;
    }

    off_t base;

    switch(origin) {
    case SEEK_END:
        size_t max_offset = 0;
        ret._errno = vop_maxseek(file->vnode, &max_offset);
        if(ret._errno)
            goto cleanup;

        if(max_offset > OFF_MAX) {
            ret._errno = EOVERFLOW;
            goto cleanup;
        }

        base = (off_t) max_offset;
        break;
    case SEEK_CUR:
        base = (off_t) file->offset;
        break;
    case SEEK_SET:
        base = 0;
        break;
    default:
        ret._errno = EINVAL;
        goto cleanup;
    }

    ret.ret = file->offset = base + offset;

cleanup:
    if(file)
        fd_release(file);

    return ret;
}

