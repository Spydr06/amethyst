#ifndef _SIGNAL_H
#define _SIGNAL_H

#ifdef __cplusplus
extern "C" {
#endif

#include <bits/alltypes.h>
#include <amethyst/signal.h>

#define _NSIG _AMETHYST_NSIG

typedef struct amethyst_sigset sigset_t;

int sigemptyset(sigset_t *set);
int sigfillset(sigset_t *set);

int sigaddset(sigset_t *set, int signo);
int sigdelset(sigset_t *set, int signo);

int sigismember(const sigset_t *set, int signo);

int sigprocmask(int how, const sigset_t *set, sigset_t *oldset);

int sigaction(int sig, const struct sigaction *restrict act, struct sigaction *restrict oldact);
void (*signal(int sig, void (*func)(int)))(int);

int kill(pid_t pid, int sig);
int raise(int sig);

#ifdef __cplusplus
}
#endif

#endif /* _SIGNAL_H */

