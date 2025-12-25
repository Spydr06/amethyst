#include <sys/syscall.h>
#include <sys/thread.h>
#include <sys/proc.h>

#include <errno.h>
#include <kernelio.h>

__syscall syscallret_t _sys_waitpid(struct cpu_context* ctx, pid_t pid, int *wstatus, int options) {
    syscallret_t ret = {
        ._errno = 0,
        .ret = -1
    };

    struct thread* thread = current_thread();
    struct proc* proc = thread->proc;

    mutex_acquire(&proc->mutex);

    if(!proc->child) {
        ret._errno = ECHILD;
        mutex_release(&proc->mutex);
        return ret;
    }

    struct proc* prev = nullptr;
    struct proc* cur = proc->child;
    struct proc* want = nullptr;

    unimplemented();

    return ret;
}

