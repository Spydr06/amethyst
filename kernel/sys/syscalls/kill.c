#include <sys/syscall.h>
#include <sys/signal.h>
#include <sys/thread.h>
#include <sys/proc.h>

#include <assert.h>
#include <errno.h>

__syscall syscallret_t _sys_kill(struct cpu_context* context, pid_t pid, signo_t signo) {
    syscallret_t ret = {
        .ret = 0,
        ._errno = -1
    };

    // TODO: fill out specifics
    struct siginfo siginfo = {0};
    siginfo.si_signo = signo;
    
    if(pid == 0) { // signal self
        assert(current_proc() != nullptr);
        ret._errno = signal_proc(current_proc(), &siginfo);
        return ret;
    }
    else if(pid < 0) {
        ret._errno = EINVAL;
        return ret;
    }

    struct proc *proc = proc_lookup(pid);
    if(!proc) {
        ret._errno = EINVAL;
        return ret;
    }

    // TODO: check process authorization
    ret._errno = signal_proc(proc, &siginfo);
    PROC_RELEASE(proc);
    return ret;
}

_SYSCALL_REGISTER(SYS_kill, _sys_kill, "kill", "%d, %d");
