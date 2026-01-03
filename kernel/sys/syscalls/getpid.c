#include <sys/syscall.h>

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

_SYSCALL_REGISTER(SYS_getpid, _sys_getpid, "getpid", "");
