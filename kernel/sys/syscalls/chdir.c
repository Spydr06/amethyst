#include <sys/syscall.h>

#include <sys/fd.h>
#include <sys/proc.h>
#include <mem/heap.h>
#include <mem/user.h>

#include <filesystem/virtual.h>

#include <errno.h>
#include <assert.h>

__syscall syscallret_t _sys_chdir(struct cpu_context* __unused, const char *pathname) {
    syscallret_t ret = {
        .ret = 0,
        ._errno = -1
    };

    size_t pathname_size;
    if((ret._errno = user_strlen(pathname, &pathname_size)))
        return ret;

    if(!pathname_size) {
        ret._errno = ENOENT;
        return ret;
    }

    char* pathname_buf = kmalloc(pathname_size + 1);
    if(!pathname_buf) {
        ret._errno = ENOMEM;
        return ret;
    }

    struct vnode* vnode = nullptr;
    struct vnode* ref = nullptr;

    pathname_buf[pathname_size] = '\0';
    ret._errno = memcpy_from_user(pathname_buf, pathname, pathname_size);
    if(ret._errno)
        goto cleanup;

    ref = pathname_buf[0] == '/' ? proc_get_root() : proc_get_cwd();
    assert(ref != nullptr);

    if((ret._errno = vfs_lookup(&vnode, ref, pathname_buf, nullptr, 0)))
        goto cleanup;

    if(vnode->type != V_TYPE_DIR) {
        ret._errno = ENOTDIR;
        goto cleanup;
    }

/*    if(vop_access(vnode, R_OK, &current_proc()->cred) != 0) {
        ret._errno = EACCES;
        goto cleanup;
    }*/

    proc_set_cwd(vnode);

cleanup:
    if(ref)
        vop_release(&ref);

    kfree(pathname_buf);
    return ret;
}

__syscall syscallret_t _sys_fchdir(struct cpu_context* __unused, int fd) {
    syscallret_t ret = {
        .ret = -1,
        ._errno = -1
    };

    struct file* file = fd_get(fd);
    if(!file) {
        ret._errno = EBADF;
        return ret;
    }

    if(!(file->flags & FILE_READ)) {
        ret._errno = EACCES;
        return ret;
    }

    struct vnode *vnode = file->vnode;
    assert(vnode);

    vop_hold(vnode);

    if(vnode->type != V_TYPE_DIR) {
        ret._errno = ENOTDIR;
        goto cleanup;
    }

    proc_set_cwd(vnode);

cleanup:
    vop_release(&vnode);
    return ret;
}

