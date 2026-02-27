#ifndef _AMETHYST_SIGNAL_H
#define _AMETHYST_SIGNAL_H

#include <stdint.h>

#define _AMETHYST_NSIG 64
#define _AMETHYST_SISET_NWORDS (1024 / 64)

#define SIG_ERR ((void*) -1)
#define SIG_DFL ((void*) 0)
#define SIG_IGN ((void*) 1)

enum amethyst_signo {
    SIGHUP = 1,
    SIGINT,
    SIGQUIT,
    SIGILL,
    SIGTRAP,
    SIGABRT,
    SIGBUS,
    SIGFPE,
    SIGKILL,
    SIGUSR1,
    SIGSEGV,
    SIGUSR2,
    SIGPIPE,
    SIGALRM,
    SIGTERM,
    SIGCHLD,
    SIGCONT,
    SIGSTOP,
};

enum amethyst_sigaction_flags {
    SA_NOCLDSTOP = 0x01,
    SA_NOCLDWAIT = 0x02,
    SA_RESTART   = 0x04,
};

struct amethyst_sigset {
    uint64_t set[_AMETHYST_SISET_NWORDS];
};

struct siginfo;

struct amethyst_sigaction {
    union {
        void (*sa_handler)(int);
        void (*sa_sigaction)(int, struct siginfo *, void *);
    } __sa_handler;
    struct amethyst_sigset sa_mask;
    enum amethyst_sigaction_flags sa_flags;
    void (*sa_restorer)(void);
};

#define sa_handler   __sa_handler.sa_handler
#define sa_sigaction __sa_hander.sa_sigaction

#if defined(_AMETHYST_KERNEL_SRC) || defined(_AMETHYST_MODULE_SRC)

typedef enum amethyst_signo signo_t;
typedef struct amethyst_sigset sigset_t;
typedef struct amethyst_sigaction sigaction_t;
typedef void (*sighandler_t)(int, struct siginfo *, void *);

#endif

#endif /* _AMETHYST_SIGNAL_H */
