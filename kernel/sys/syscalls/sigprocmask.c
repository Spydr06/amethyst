#include <sys/syscall.h>

#include <amethyst/signal.h>
#include <mem/user.h>
#include <sys/sigset.h>
#include <sys/spinlock.h>
#include <sys/thread.h>

#include <errno.h>

__syscall syscallret_t _sys_sigprocmask(struct cpu_context *__unused, int how, sigset_t *set, sigset_t *oldset) {
    syscallret_t ret = {
        .ret = -1
    };

    struct thread *thread = current_thread();
    spinlock_acquire(&thread->sig_mask_lock);

    if(oldset) {
        if((ret._errno = memcpy_to_user(oldset, (const sigset_t*) &thread->sig_mask, sizeof(sigset_t))))
            goto cleanup;
    }

    if(set) {
        sigset_t set_copy;
        if((ret._errno = memcpy_from_user(&set_copy, set, sizeof(sigset_t))))
            goto cleanup;

        switch(how) {
        case SIG_BLOCK:
            sigset_block(&thread->sig_mask, &set_copy);
            break;
        case SIG_UNBLOCK:
            sigset_unblock(&thread->sig_mask, &set_copy);
            break;
        case  SIG_SETMASK:
            sigset_setmask(&thread->sig_mask, &set_copy);
            break;
        default:
            ret._errno = EINVAL;
        }
    }
    
cleanup:
    spinlock_release(&thread->sig_mask_lock);
    return ret;
}

_SYSCALL_REGISTER(SYS_sigprocmask, _sys_sigprocmask, "sigprocmask", "%d, %p, %p");

