#include <sys/syscall.h>

#include <sys/proc.h>
#include <sys/fd.h>
#include <filesystem/virtual.h>
#include <mem/user.h>
#include <mem/heap.h>

#include <assert.h>
#include <errno.h>

__syscall syscallret_t _sys_open(struct cpu_context* __unused, const char *path, int flags, mode_t mode) {
    syscallret_t ret = {
        .ret = -1,
        ._errno = -1
    };

    flags++;

    size_t path_size;
    ret._errno = user_strlen(path, &path_size);
    if(ret._errno)
        return ret;

    char* path_buf = kmalloc(path_size + 1);
    if(!path_buf) {
        ret._errno = ENOMEM;
        return ret;
    }

    path_buf[path_size] = '\0';
    ret._errno = memcpy_from_user(path_buf, path, path_size);
    if(ret._errno) {
        kfree(path_buf);
        return ret;
    }

    struct vnode* ref = path_buf[0] == '/' ? proc_get_root() : proc_get_cwd();
    assert(ref != nullptr);

retry:
    struct file* new_file = nullptr;
    int new_fd;
    ret._errno = fd_new(flags & O_CLOEXEC, &new_file, &new_fd);
    if(ret._errno)
        goto cleanup;

    struct vnode* vnode = nullptr;
    ret._errno = vfs_open(ref, path_buf, file_to_vnode_flags(flags), &vnode);
    if(ret._errno == 0 && (flags & O_CREAT) && (flags & O_EXCL)) {
        ret._errno = EEXIST;
        goto cleanup;
    }

    if(ret._errno == ENOENT && (flags & O_CREAT)) {
        // Create a new file
        struct vattr attr = {
            .mode = umask(mode),
            .gid = current_proc()->cred.gid,
            .uid = current_proc()->cred.uid
        };

        ret._errno = vfs_create(ref, path_buf, &attr, V_TYPE_REGULAR, &vnode);
        if(ret._errno == EEXIST && (flags & O_EXCL) == 0)
            goto retry;
        if(ret._errno == 0)
            vop_unlock(vnode);
    }

    if(ret._errno)
        goto cleanup;

    if((flags & O_DIRECTORY) && vnode->type != V_TYPE_DIR) {
        ret._errno = ENOTDIR;
        goto cleanup;
    }

    struct vattr attr;
    vop_lock(vnode);
    ret._errno = vop_getattr(vnode, &attr, &current_proc()->cred);
    vop_unlock(vnode);
    if(ret._errno)
        goto cleanup;

    if(vnode->type == V_TYPE_REGULAR && (flags & O_TRUNC) && (flags & FILE_WRITE)) {
        mutex_acquire(&vnode->size_lock, false);
        vop_lock(vnode);

        ret._errno = vop_resize(vnode, 0, &current_proc()->cred);

        vop_unlock(vnode);
        mutex_release(&vnode->size_lock);
        if(ret._errno)
            goto cleanup;
    }

    new_file->vnode = vnode;
    new_file->flags = flags;
    new_file->offset = 0;
    new_file->mode = attr.mode;

    ret._errno = 0;
    ret.ret = new_fd;

cleanup:
    if(vnode && ret._errno) {
        vfs_close(vnode, file_to_vnode_flags(flags));
        vop_release(&vnode);
    }

    if(ref)
        vop_release(&ref);

    if(new_file && ret._errno)
        assert(fd_close(new_fd) == 0);

    if(path_buf)
        kfree(path_buf);

    return ret;
}

