#include <sys/syscall.h>
#include <sys/thread.h>
#include <sys/proc.h>
#include <sys/fd.h>
#include <filesystem/virtual.h>
#include <amethyst/stat.h>

#include <mem/heap.h>
#include <mem/user.h>

#include <assert.h>
#include <errno.h>

__syscall syscallret_t stat_vnode(struct vnode *vnode, struct stat *statbuf) {
    syscallret_t ret = {
        .ret = 0,
        ._errno = -1
    };

    struct vattr attr;
    memset(&attr, 0, sizeof(struct vattr));

    ret._errno = vop_getattr(vnode, &attr, &_cpu()->thread->proc->cred);
    if(ret._errno)
        return ret;

    struct stat stat;
    memset(&stat, 0, sizeof(struct stat));

    stat.dev = TO_DEV(attr.dev_major, attr.dev_minor);
    stat.rdev = TO_DEV(attr.rdev_major, attr.rdev_minor);
    stat.ino = attr.inode;
    stat.nlink = attr.nlinks;
    stat.mode = attr.mode;
    stat.uid = attr.uid;
    stat.gid = attr.gid;
    stat.size = attr.size;
    stat.blksize = attr.fsblock_size;
    stat.blocks = attr.blocks_used;
    stat.atim = attr.atime;
    stat.mtim = attr.mtime;
    stat.ctim = attr.ctime;

    ret._errno = memcpy_to_user(statbuf, &stat, sizeof(struct stat));
    return ret;
}

__syscall syscallret_t _sys_stat(struct cpu_context* __unused, const char* path, struct stat *statbuf) {
    syscallret_t ret = {
        .ret = -1,
        ._errno = -1
    };

    struct vnode* vnode = nullptr;

    size_t path_size;
    if((ret._errno = user_strlen(path, &path_size)))
        return ret;

    char* path_buf = kmalloc(path_size + 1);
    if((ret._errno = memcpy_from_user(path_buf, path, path_size)))
        goto cleanup;

    struct vnode* cwd = _cpu()->thread->proc->cwd;
    assert(cwd != nullptr);

    ret._errno = vfs_lookup(&vnode, cwd, path_buf, nullptr, 0);
    if(ret._errno)
        goto cleanup;

    ret = stat_vnode(vnode, statbuf);

cleanup:
    if(vnode)
        vop_release(&vnode);
    kfree(path_buf);
    return ret;
}

__syscall syscallret_t _sys_fstat(struct cpu_context* __unused, int fd, struct stat *statbuf) {
    syscallret_t ret = {
        .ret = -1,
        ._errno = -1
    };

    struct file *file = fd_get(fd);
    if(!file) {
        ret._errno = EBADF;
        return ret;
    }

    assert(file->vnode != nullptr);
    ret = stat_vnode(file->vnode, statbuf);

    fd_release(file);
    return ret;
}
