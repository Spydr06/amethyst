#include <sys/syscall.h>

#include <sys/thread.h>
#include <sys/proc.h>
#include <errno.h>

syscallret_t _sys_getpid(struct cpu_context*) {
    struct proc* proc = current_proc();

    syscallret_t ret = {
        ._errno = proc ? 0 : EINVAL,
        .ret = proc ? proc->pid : 0
    };
    return ret;
}

syscallret_t _sys_gettid(struct cpu_context*) {
    struct thread *thread = current_thread();

    syscallret_t ret = {
        ._errno = thread ? 0 : EINVAL,
        .ret = thread ? thread->tid : 0
    };
    return ret;
}

_SYSCALL_REGISTER(SYS_getpid, _sys_getpid, "getpid", "");
_SYSCALL_REGISTER(SYS_gettid, _sys_gettid, "gettid", "");
