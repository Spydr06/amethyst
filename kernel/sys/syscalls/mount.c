#include "filesystem/virtual.h"
#include "sys/proc.h"
#include <sys/syscall.h>

#include <mem/heap.h>
#include <mem/user.h>
#include <errno.h>

__syscall syscallret_t _sys_mount(struct cpu_context* __unused, char* u_backing, char* u_dir_name, char* u_type, register_t __unused, void* data) {
    syscallret_t ret = {
        .ret = 0
    };

    size_t dir_name_len, type_len;
    if(user_strlen(u_dir_name, &dir_name_len) || user_strlen(u_type, &type_len)) {
        ret._errno = EFAULT;
        return ret;
    }

    char* dir_name = kmalloc(dir_name_len + 1);
    char* type = kmalloc(type_len + 1);

    if(!dir_name || !type_len) {
        if(dir_name)
            kfree(dir_name);
        if(type_len)
            kfree(type);
            
        ret._errno = ENOMEM;
        return ret;
    }

    struct vnode* backing_ref_node = nullptr;
    struct vnode* mount_point_ref = nullptr;
    struct vnode* backing_node = nullptr;

    if(memcpy_from_user(type, u_type, type_len) || memcpy_from_user(dir_name, u_dir_name, dir_name_len)) {
        ret._errno = EFAULT;
        goto cleanup;
    }

    char* backing = nullptr;
    if(u_backing) {
        size_t backing_len;
        if(user_strlen(u_backing, &backing_len)) {
            ret._errno = EFAULT;
            goto cleanup;
        }

        if(!(backing = kmalloc(backing_len + 1))) {
            ret._errno = EFAULT;
            goto cleanup;
        }

        if(memcpy_from_user(backing, u_backing, backing_len)) {
            ret._errno = EFAULT;
            goto cleanup;
        }
    }

    if(backing)
        backing_ref_node = backing[0] == '/' ? proc_get_root() : proc_get_cwd();
    mount_point_ref = dir_name[0] == '/' ? proc_get_root() : proc_get_cwd();

    if(backing) {
        ret._errno = vfs_lookup(&backing_node, backing_ref_node, backing, nullptr, 0);
        if(ret._errno)
            goto cleanup;

        vop_unlock(backing_node);
    }

    ret._errno = vfs_mount(backing_node, mount_point_ref, dir_name, type, data);
    if(backing)
        vop_release(&backing_node);

cleanup:
    if(backing_ref_node)
        vop_release(&backing_ref_node);

    if(mount_point_ref)
        vop_release(&mount_point_ref);

    if(backing)
        kfree(backing);

    kfree(dir_name);
    kfree(type);

    return ret;
}

__syscall syscallret_t _sys_umount(struct cpu_context* __unused, const char* u_dir_name) {
    syscallret_t ret = {
        .ret = 0
    };

    size_t dir_name_len;
    if(user_strlen(u_dir_name, &dir_name_len)) {
        ret._errno = EINVAL;
        return ret;
    }

    char* dir_name = kmalloc(dir_name_len + 1);
    if(!dir_name) {
        ret._errno = ENOMEM;
        return ret;
    }

    struct vnode* dir_ref_node = nullptr;

    if(memcpy_from_user(dir_name, u_dir_name, dir_name_len + 1))
        goto cleanup;

    dir_ref_node = dir_name[0] == '/' ? proc_get_root() : proc_get_cwd();

    ret._errno = vfs_umount(dir_ref_node, dir_name);

cleanup:
    if(dir_ref_node)
        vop_release(&dir_ref_node);

    kfree(dir_name);
    return ret;
}

