#include <sys/syscall.h>

#include <amethyst/signal.h>
#include <sys/proc.h>
#include <mem/user.h>

#include <errno.h>

__syscall syscallret_t _sys_sigaction(struct cpu_context* __unused, int signo, const sigaction_t *action, sigaction_t *old_action) {
    syscallret_t ret = {
        .ret = -1
    };

    if(signo <= 0 || signo > _AMETHYST_NSIG) {
        ret._errno = EINVAL;
        return ret;
    }

    struct proc *proc = current_proc();
    spinlock_acquire(&proc->sig_actions_lock);

    if(old_action) {
        if((ret._errno = memcpy_to_user(old_action, &proc->sig_actions[signo - 1], sizeof(sigaction_t))))
            goto cleanup;
    }

    if(action) {
        if((ret._errno = memcpy_from_user(&proc->sig_actions[signo - 1], action, sizeof(sigaction_t))))
            goto cleanup;
    }

cleanup:
    spinlock_release(&proc->sig_actions_lock);
    return ret;
}

_SYSCALL_REGISTER(SYS_sigaction, _sys_sigaction, "sigaction", "%d, %p, %p");

