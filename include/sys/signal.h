#ifndef _AMETHYST_SYS_SIGINFO_H
#define _AMETHYST_SYS_SIGINFO_H

#include <amethyst/signal.h>
#include <io/poll.h>
#include <abi.h>
#include <sys/spinlock.h>

#include <cpu/cpu.h>
#include <stdint.h>

#define SIGNAL_NUM_PRIORITIES 3

#define SIGHANDLER_TERM sighandler_term
#define SIGHANDLER_IGN  sighandler_ign
#define SIGHANDLER_CORE sighandler_core
#define SIGHANDLER_STOP sighandler_stop
#define SIGHANDLER_CONT sighandler_cont

enum signal_priority {
    SIGNAL_PRIO_DEFAULT = 0,
    SIGNAL_PRIO_URGENT, // SIGSEGV, SIGILL, SIGFPE, ...
    SIGNAL_PRIO_KILL    // SIGKILL
};

union sigattrs {
    struct { // SIGKILL
        pid_t pid;
        uid_t uid;
    } kill;

    struct { // SIGRT
        pid_t pid;
        uid_t uid;
        int exit_status;
    } chld;

    struct { // SIGILL, SIGFPE, SIGSEGV, SIGTRAP
        uintptr_t addr;
    } fault;

    struct { // SIGPOLL
        int fd;
        enum io_poll_event event;
    } poll;
};

struct siginfo {
    signo_t si_signo;
    int si_errno;
    int si_code;
    union sigattrs si_attrs;
};

struct sig_queue {
    spinlock_t lock;
    volatile sigset_t pending;
    volatile struct siginfo signals[_AMETHYST_NSIG];
};

extern const char *signal_strtable[_AMETHYST_NSIG + 1];
extern const char unknown_signal[];

extern const sighandler_t sighandler_default[_AMETHYST_NSIG + 1];
extern const enum signal_priority signal_priorities[_AMETHYST_NSIG + 1];

bool dispatch_signal(struct thread *thread, struct cpu_context *context, bool syscall, register_t syscall_errno, register_t syscall_ret);

int sig_queue_init(struct sig_queue* queue);
void sig_queue_delete(struct sig_queue *queue);

int signal_raise(struct sig_queue *queue, const struct siginfo *sig);
int signal_acquire(struct sig_queue *queue, struct siginfo *sig, const sigset_t *mask);
bool signal_pending(struct sig_queue *queue);

struct proc;
int signal_proc(struct proc *proc, struct siginfo *sig);

void sighandler_term(int sig, struct siginfo *info, void *p);
void sighandler_ign(int sig, struct siginfo *info, void *p);
void sighandler_core(int sig, struct siginfo *info, void *p);
void sighandler_stop(int sig, struct siginfo *info, void *p);
void sighandler_cont(int sig, struct siginfo *info, void *p);


static inline const char *strsignal(signo_t signo) {
    if(signo < 0 || signo > _AMETHYST_NSIG)
        return unknown_signal;
    return __either(signal_strtable[signo], unknown_signal);
}

#endif /* _AMETHYST_SYS_SIGINFO_H */
