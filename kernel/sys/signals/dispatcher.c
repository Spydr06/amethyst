#include "amethyst/amethyst.h"
#include <amethyst/signal.h>

#include <sys/proc.h>
#include <sys/scheduler.h>
#include <sys/signal.h>
#include <sys/sigset.h>
#include <sys/thread.h>

#include <assert.h>
#include <kernelio.h>

static int fetch_signal(struct thread *thread, struct siginfo *sig) {
    for(int i = SIGNAL_NUM_PRIORITIES - 1; i >= 0; i--) {
        if(signal_acquire(thread->sig_queue + i, sig, &thread->sig_mask) == 0)
            return i;
    }

    return -1;
}

bool dispatch_signal(struct thread *thread, struct cpu_context *context, bool syscall, register_t syscall_errno, register_t syscall_ret) {
    struct siginfo sig;
    int priority;
    if((priority = fetch_signal(thread, &sig)) < 0)
        return false; // no signal pending

    assert(thread->proc && !thread->should_exit);
    klog(ERROR, "[tid %u] received signal '%s' (%d)", thread->tid, strsignal(sig.si_signo), sig.si_signo);

    if(sig.si_signo < 0 || sig.si_signo >= _AMETHYST_NSIG)
        return true; // invalid signal number

    if(sig.si_signo == SIGKILL) { // SIGKILL always terminates
        scheduler_terminate(PROC_STATUS_SIGNALLED(sig.si_signo));
        unreachable();
    }

    sighandler_t handler = thread->proc->sig_handlers[sig.si_signo];
    if(handler == SIG_IGN) {
        if(priority >= SIGNAL_PRIO_URGENT)
            goto default_handler; // cannot ignore urgent signals
        
        return true; // ignore signal
    }

    if(handler == SIG_DFL) {
default_handler:
        sighandler_t handler = sighandler_default[sig.si_signo];
        if(!handler)
            panic("No default handler for signal %d", sig.si_signo);
        
        handler(sig.si_signo, &sig, nullptr);
    }
    else if(is_userspace_addr(handler)) {
        unimplemented();
    }
    else { // invalid signal handler
        unimplemented();
    }

    return true;
}
