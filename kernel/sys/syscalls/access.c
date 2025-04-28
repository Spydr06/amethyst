#include <sys/syscall.h>
#include <sys/proc.h>

#include <mem/heap.h>
#include <mem/user.h>

#include <assert.h>
#include <errno.h>

__syscall syscallret_t _sys_access(struct cpu_context* ctx, const char* filename, mode_t mode) {
    syscallret_t ret = {
        .ret = -1,
        ._errno = -1
    };

    size_t filename_size;
    ret._errno = user_strlen(filename, &filename_size);
    if(ret._errno)
        return ret;

    if(!filename_size) {
        ret._errno = ENOENT;
        return ret;
    }

    char* filename_buf = kmalloc(filename_size + 1);
    if(!filename_buf) {
        ret._errno = ENOMEM;
        return ret;
    }

    struct vnode* vnode = nullptr;

    filename_buf[filename_size] = '\0';
    ret._errno = memcpy_from_user(filename_buf, filename, filename_size);
    if(ret._errno)
        goto cleanup;

    struct vnode* ref = filename_buf[0] == '/' ? proc_get_root() : proc_get_cwd();
    assert(ref != nullptr);

    ret._errno = vfs_lookup(&vnode, ref, filename_buf, nullptr, 0);
    if(ret._errno)
        goto cleanup;

    ret.ret = vop_access(vnode, mode, &current_proc()->cred);

cleanup:
    kfree(filename_buf);
    return ret;
}
