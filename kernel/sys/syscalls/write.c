#include <sys/syscall.h>

#include <sys/proc.h>
#include <sys/fd.h>
#include <mem/user.h>
#include <mem/vmm.h>

#include <errno.h>
#include <math.h>

__syscall syscallret_t _sys_write(struct cpu_context* __unused, int fd, void* buffer, size_t size) {
    syscallret_t ret = {
        .ret = -1,
        ._errno = 0
    };

    void* kernel_buff = vmm_map(nullptr, MAX(size, 1), VMM_FLAGS_ALLOCATE, MMU_FLAGS_READ | MMU_FLAGS_WRITE | MMU_FLAGS_NOEXEC, nullptr);
    if(!kernel_buff) {
        ret._errno = ENOMEM;
        return ret;
    }

    struct file* file = fd_get((size_t) fd);

    if(!file || !(file->flags & FILE_WRITE)) {
        ret._errno = EBADF;
        goto cleanup;
    }

    ret._errno = memcpy_from_user(kernel_buff, buffer, size);
    if(ret._errno)
        goto cleanup;

    if(!size) {
        ret.ret = 0;
        goto cleanup;
    }

    size_t bytes_written = 0;
    uintmax_t offset = file->offset;

    if(file->flags & O_APPEND) {
        struct vattr attr;

        vop_lock(file->vnode);
        ret._errno = vop_getattr(file->vnode, &attr, &_cpu()->thread->proc->cred);
        vop_release(&file->vnode);
        if(ret._errno)
            goto cleanup;

        offset = attr.size;
    }

    ret._errno = vfs_write(file->vnode, kernel_buff, size, offset, &bytes_written, file_to_vnode_flags(file->flags));
    if(ret._errno)
        goto cleanup;

    file->offset = offset + bytes_written;
    ret.ret = bytes_written;

cleanup:
    if(file)
        fd_release(file);

    vmm_unmap(kernel_buff, MAX(size, 1), 0);
    return ret;
}

