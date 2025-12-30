#include "sys/scheduler.h"
#include <sys/syscall.h>

#include <mem/vmm.h>
#include <mem/user.h>
#include <mem/heap.h>

#include <sys/thread.h>
#include <sys/proc.h>

#include <encoding/elf.h>

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

__syscall syscallret_t _sys_execve(struct cpu_context* ctx, const char *u_path, const char *const u_argv[], const char *const u_envp[]) {
    syscallret_t ret = {
        .ret = -1,
        ._errno = 0
    };

    if(!is_userspace_addr(u_path) || !is_userspace_addr(u_argv) || !is_userspace_addr(u_envp)) {
        ret._errno = EFAULT;
        return ret;
    }

    size_t path_len;
    if((ret._errno = user_strlen(u_path, &path_len)))
        return ret;

    char* path = kmalloc(path_len + 1);
    if(!path) {
        ret._errno = ENOMEM;
        return ret;
    }

    if((ret._errno = memcpy_from_user(path, u_path, path_len))) {
        kfree(path);
        return ret;
    }

    size_t argc = 0;
    size_t envc = 0;
    if((ret._errno = count_args(u_argv, &argc)) || (ret._errno = count_args(u_envp, &envc)))
        return ret;

    char** argv = kmalloc((argc + 1) * sizeof(char*));
    char** envp = kmalloc((envc + 1) * sizeof(char*));

    Elf64_auxv_list_t auxv;
    char* interpreter = nullptr;
    void* entry;
    void* brk = nullptr;

    struct vmm_context* old_vmm_ctx = current_vmm_context();
    struct vmm_context* vmm_ctx = nullptr;

    struct vnode* ref = *path == '/' ? proc_get_root() : proc_get_cwd();
    struct vnode* node = nullptr;


    if(argv == nullptr || envp == nullptr) {
        ret._errno = ENOMEM;
        goto cleanup;
    }

    if((ret._errno = copy_args(argv, u_argv, argc)) || (ret._errno = copy_args(envp, u_envp, envc)))
        goto cleanup;

    if((ret._errno = vfs_lookup(&node, ref, path, nullptr, 0)))
        goto cleanup;

    if(!(vmm_ctx = vmm_context_new())) {
        ret._errno = ENOMEM;
        goto cleanup;
    }

    vmm_ctx->brk = (struct brk){
        .base = brk,
        .top = brk
    };

    vmm_switch_context(vmm_ctx);

    if((ret._errno = elf_load(node, nullptr, &entry, &interpreter, &auxv, &brk)))
        goto cleanup;

    // TODO: load interpreter -> shebang support!
    
    void* stack = elf_prepare_stack(STACK_TOP, &auxv, argv, envp);
    if(!stack) {
        ret._errno = ENOMEM;
        goto cleanup;
    }

    struct vattr vattr;
    vop_lock(node);
    ret._errno = vop_getattr(node, &vattr, nullptr);
    vop_unlock(node);
    if(ret._errno)
        goto cleanup;

    sched_stop_other_threads();

    struct proc *proc = current_proc();
    for(size_t fd = 0; fd < proc->fd_count; fd++) {
        if(proc->fd[fd].flags & O_CLOEXEC)
            fd_close((int) fd);
    }

    // TODO: clear signals
    
    vmm_context_destroy(old_vmm_ctx);
    CPU_SP(ctx) = (register_t) stack;
    CPU_IP(ctx) = (register_t) entry;

cleanup:
    kfree(path);
    
    if(argv)
        kfree(argv);
    if(envp)
        kfree(envp);

    if(vmm_ctx && ret._errno) {
        vmm_switch_context(old_vmm_ctx);
        vmm_context_destroy(vmm_ctx);
    }

    if(ref)
        vop_release(&ref);

    if(node)
        vop_release(&node);

    if(interpreter)
        kfree(interpreter);

    return ret;
}

_SYSCALL_REGISTER(SYS_execve, _sys_execve, "execve", "%p, %p, %p");
