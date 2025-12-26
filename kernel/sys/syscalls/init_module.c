#include "init/module.h"
#include <sys/syscall.h>

#include <mem/user.h>
#include <mem/heap.h>
#include <sys/fd.h>

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

_SYSCALL_REGISTER(SYS_finit_module, _sys_finit_module, "finit_module", "%d, %p, 0x%x");

