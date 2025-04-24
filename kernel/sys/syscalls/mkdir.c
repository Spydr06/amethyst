#include <sys/syscall.h>
#include <sys/proc.h>

#include <mem/user.h>
#include <mem/heap.h>

#include <errno.h>

__syscall syscallret_t _sys_mkdir(struct cpu_context* __unused, const char* u_path, mode_t mode) {
    syscallret_t ret = {
        .ret = -1
    };

    size_t path_len;
    ret._errno = user_strlen(u_path, &path_len);
    if(ret._errno)
        return ret;

    char *path = kmalloc(path_len + 1);
    if(!path) {
        ret._errno = ENOMEM;
        return ret;
    }

    struct vnode* dir_ref_node = nullptr;

    ret._errno = memcpy_from_user(path, u_path, path_len + 1);
    if(ret._errno)
        goto cleanup;

    dir_ref_node = path[0] == '/' ? proc_get_root() : proc_get_cwd();
    vop_hold(dir_ref_node);

    struct vattr attr = {
        .mode = umask(mode),
        .gid = current_proc()->cred.gid,
        .uid = current_proc()->cred.uid
    };

    ret._errno = vfs_create(dir_ref_node, path, &attr, V_TYPE_DIR, NULL);
    ret.ret = ret._errno ? -1 : 0;

cleanup:
    if(dir_ref_node)
        vop_release(&dir_ref_node);

    kfree(path);
    return ret;
}

