#include <amethyst/signal.h>
#include <amethyst/amethyst.h>

#include <sys/coredump.h>
#include <sys/proc.h>
#include <sys/scheduler.h>
#include <sys/signal.h>
#include <sys/spinlock.h>

#include <errno.h>

const char unknown_signal[] = "<unknown signal>";

const char *signal_strtable[_AMETHYST_NSIG + 1] = {
    [0] = "Unknown signal 0",
    [SIGHUP]  = "Hangup",
    [SIGINT]  = "Interrupt",
    [SIGQUIT] = "Quit",
    [SIGILL]  = "Illegal instruction",
    [SIGTRAP] = "Trace/breakpoint trap",
    [SIGABRT] = "Aborted",
    [SIGBUS]  = "Bus error",
    [SIGFPE]  = "Floating point exception",
    [SIGKILL] = "Killed",
    [SIGUSR1] = "User defined signal 1",
    [SIGSEGV] = "Segmentation fault",
    [SIGUSR2] = "User defined signal 2",
    [SIGPIPE] = "Broken pipe",
    [SIGALRM] = "Alarm clock",
    [SIGTERM] = "Terminated",
    [SIGCHLD] = "Child exited",
    [SIGCONT] = "Continued",
    [SIGSTOP] = "Stopped",
};

const sighandler_t sighandler_default[_AMETHYST_NSIG + 1] = {
    [SIGHUP]  = SIGHANDLER_TERM,
    [SIGINT]  = SIGHANDLER_TERM,
    [SIGQUIT] = SIGHANDLER_CORE,
    [SIGILL]  = SIGHANDLER_CORE,
    [SIGTRAP] = SIGHANDLER_CORE,
    [SIGABRT] = SIGHANDLER_CORE,
    [SIGBUS]  = SIGHANDLER_CORE,
    [SIGFPE]  = SIGHANDLER_CORE,
    [SIGKILL] = SIGHANDLER_TERM,
    [SIGUSR1] = SIGHANDLER_TERM,
    [SIGSEGV] = SIGHANDLER_CORE,
    [SIGUSR2] = SIGHANDLER_TERM,
    [SIGPIPE] = SIGHANDLER_TERM,
    [SIGALRM] = SIGHANDLER_TERM,
    [SIGTERM] = SIGHANDLER_TERM,
    [SIGCHLD] = SIGHANDLER_IGN,
    [SIGCONT] = SIGHANDLER_CONT,
    [SIGSTOP] = SIGHANDLER_STOP,
};

const enum signal_priority signal_priorities[_AMETHYST_NSIG + 1] = {
    [SIGHUP]  = SIGNAL_PRIO_DEFAULT,
    [SIGINT]  = SIGNAL_PRIO_URGENT,
    [SIGQUIT] = SIGNAL_PRIO_DEFAULT,
    [SIGILL]  = SIGNAL_PRIO_URGENT,
    [SIGTRAP] = SIGNAL_PRIO_URGENT,
    [SIGABRT] = SIGNAL_PRIO_URGENT,
    [SIGBUS]  = SIGNAL_PRIO_DEFAULT,
    [SIGFPE]  = SIGNAL_PRIO_URGENT,
    [SIGKILL] = SIGNAL_PRIO_KILL,
    [SIGUSR1] = SIGNAL_PRIO_DEFAULT,
    [SIGSEGV] = SIGNAL_PRIO_URGENT,
    [SIGUSR2] = SIGNAL_PRIO_DEFAULT,
    [SIGPIPE] = SIGNAL_PRIO_DEFAULT,
    [SIGALRM] = SIGNAL_PRIO_DEFAULT,
    [SIGTERM] = SIGNAL_PRIO_URGENT,
    [SIGCHLD] = SIGNAL_PRIO_DEFAULT,
    [SIGCONT] = SIGNAL_PRIO_DEFAULT,
    [SIGSTOP] = SIGNAL_PRIO_URGENT,
};

int signal_thread(struct thread *thread, struct siginfo *sig) {
    if(sig->si_signo > _AMETHYST_NSIG)
        return EINVAL;
    return signal_raise(&thread->sig_queue[signal_priorities[sig->si_signo]], sig);
}

int signal_proc(struct proc *proc, struct siginfo *sig) {
    bool int_save = interrupt_set(false);
    PROC_HOLD(proc);

    int err = 0;
    for(struct thread *thread = proc->threads.head; thread; thread = thread->proc_next) {
        if(thread->flags & THREAD_FLAGS_DEAD)
            continue;

        if((err = signal_thread(thread, sig)) == 0)
            break; // signal successfully raised
    }

    PROC_RELEASE(proc);
    interrupt_set(int_save);
    return err;
}

void sighandler_term(int sig, struct siginfo *, void *) {
    struct proc *proc = current_proc();

    proc->status = PROC_STATUS_SIGNALLED(sig);
    spinlock_acquire(&proc->exiting);

    for(struct thread *thread = proc->threads.head; thread; thread = thread->proc_next) {
        thread->should_exit = true;
    }

    scheduler_terminate(PROC_STATUS_SIGNALLED(sig));
    unreachable();
}

void sighandler_ign(int sig, struct siginfo *info, void *p) {
    (void) sig;
    (void) info;
    (void) p;
    unimplemented();
}

void sighandler_core(int sig, struct siginfo *info, void *) {
    core_dump(current_proc(), info);
    scheduler_terminate(PROC_STATUS_SIGNALLED(sig));
}

void sighandler_stop(int sig, struct siginfo *info, void *p) {
    (void) sig;
    (void) info;
    (void) p;
    unimplemented();
}

void sighandler_cont(int sig, struct siginfo *info, void *p) {
    (void) sig;
    (void) info;
    (void) p;
    unimplemented();
}
