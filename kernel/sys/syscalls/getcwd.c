#include <sys/syscall.h>

#include <sys/thread.h>
#include <sys/proc.h>

#include <mem/user.h>
#include <mem/heap.h>

#include <errno.h>
#include <math.h>

__syscall syscallret_t _sys_getcwd(struct cpu_context* __unused, char* user_cwd, size_t user_cwd_size) {
    syscallret_t ret = {
        .ret = -1,
        ._errno = -1
    };

    struct proc* proc = current_proc();
    if(!proc) {
        ret._errno = EINVAL;
        return ret;
    }

    struct vnode *cwd_node = proc->cwd;
    if(!cwd_node) {
        ret._errno = ENOENT;
        return ret;
    }

    size_t cwd_size = 0;
    char* cwd = nullptr;
    if((ret._errno = vfs_realpath(cwd_node, &cwd, &cwd_size, VFS_LOOKUP_NOLINK)))
        return ret;

    cwd_size = MIN(cwd_size, user_cwd_size - 1);
    
    if((ret._errno = memcpy_to_user(user_cwd, cwd, cwd_size)))
        goto cleanup;

    ret.ret = cwd_size;
cleanup:
    kfree(cwd);
    return ret;
}

