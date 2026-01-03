#include "init/module.h"
#include "sys/proc.h"
#include <sys/syscall.h>

#include <amethyst/cred.h>
#include <mem/user.h>
#include <mem/heap.h>
#include <sys/fd.h>

#include <assert.h>
#include <errno.h>

static int count_args(const char *const u_args[], size_t* argc) {
    int err; 
    for(;;) {
        char* arg;
        if((err = memcpy_from_user(&arg, &u_args[*argc], sizeof(char*))))
            return err;
        if(arg == NULL)
            break;
        (*argc)++;
    }
    return 0;
}

static int copy_args(char** dst, const char *const u_args[], size_t argc) {
    int err;
    for(size_t i = 0; i < argc; i++) {
        char* u_arg;
        if((err = memcpy_from_user(&u_arg, &u_args[i], sizeof(char*))))
            return err;

        if(!is_userspace_addr(u_arg))
            return EFAULT;
        
        size_t arg_len;
        if((err = user_strlen(u_arg, &arg_len)))
            return err;

        dst[i] = kmalloc(arg_len + 1);
        if(!dst[i])
            return ENOMEM;
        
        if((err = memcpy_from_user(dst[i], u_arg, arg_len)))
            return err;
    }

    return 0;
}

static int check_perms(void) {
    struct proc *proc = current_proc();
    assert(proc != nullptr);

    return proc->cred.uid == AMETHYST_ROOT_UID && proc->cred.gid == AMETHYST_ROOT_GID ? 0 : EPERM;
}

__syscall syscallret_t _sys_init_module(struct cpu_context* __unused, const char *path, char **u_args, int flags) {
    syscallret_t ret = {
        ._errno = 0,
        .ret = -1
    };

    if((ret._errno = check_perms()))
        return ret;

    if(!is_userspace_addr(u_args)) {
        ret._errno = EFAULT;
        return ret;
    }

    struct vnode* vnode = nullptr, *ref = nullptr;
    char** args = nullptr;

    size_t path_size;
    if((ret._errno = user_strlen(path, &path_size)))
        return ret;

    char* path_buf = kmalloc(path_size + 1);
    if(!path_buf) {
        ret._errno = ENOMEM;
        goto cleanup;
    }

    if((ret._errno = memcpy_from_user(path_buf, path, path_size)))
        goto cleanup;

    ref = path_buf[0] == '/' ? proc_get_root() : proc_get_cwd();
    assert(ref != nullptr);

    if((ret._errno = vfs_lookup(&vnode, ref, path_buf, nullptr, 0)))
        goto cleanup;

    size_t argc = 0;
    if((ret._errno = count_args((const char* const*) u_args, &argc)))
        return ret;

    if(!(args = kmalloc((argc + 1) * sizeof(char*)))) {
        ret._errno = ENOMEM;
        goto cleanup;
    }

    if((ret._errno = copy_args(args, (const char* const*) u_args, argc)))
        goto cleanup;

    if((ret._errno = kmodule_load(vnode, argc, args, flags)))
        goto cleanup;
    
    ret.ret = 0;
    return ret;
cleanup:
    if(vnode)
        vop_release(&vnode);
    if(ref)
        vop_release(&ref);
    kfree(args);
    kfree(path_buf);
    return ret;
}

__syscall syscallret_t _sys_finit_module(struct cpu_context* __unused, int fd, char **u_args, int flags) {
    syscallret_t ret = {
        ._errno = 0,
        .ret = -1
    };

    if(!is_userspace_addr(u_args)) {
        ret._errno = EFAULT;
        return ret;
    }

    struct file* file = fd_get(fd);
    if(!file || (file->flags & FILE_READ) == 0) {
        ret._errno = EBADF;
        return ret;
    }

    size_t argc = 0;
    if((ret._errno = count_args((const char* const*) u_args, &argc)))
        return ret;

    char** args = kmalloc((argc + 1) * sizeof(char*));
    if(!args) {
        ret._errno = ENOMEM;
        goto cleanup;
    }

    if((ret._errno = copy_args(args, (const char* const*) u_args, argc)))
        goto cleanup;

    if((ret._errno = kmodule_load(file->vnode, argc, args, flags)))
        goto cleanup;
    
    ret.ret = 0;
    return ret;
cleanup:
    kfree(args);
    fd_release(file);
    return ret;
}

_SYSCALL_REGISTER(SYS_init_module, _sys_init_module, "init_module", "%p, %p, 0x%x");
_SYSCALL_REGISTER(SYS_finit_module, _sys_finit_module, "finit_module", "%d, %p, 0x%x");

