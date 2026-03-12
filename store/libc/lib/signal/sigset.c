#include <signal.h>
#include <unistd.h>

#include <bits/alltypes.h>

#include <sys/syscall.h>
#include <internal/syscall.h>

#include <errno.h>
#include <memory.h>

int sigprocmask(int how, const sigset_t *set, sigset_t *oldset) {
    return syscall(SYS_sigprocmask, how, set, oldset);
}

int sigemptyset(sigset_t *set) {
    memset(set, 0, sizeof(sigset_t));
}

int sigfillset(sigset_t *set) {
    memset(set, 0xff, sizeof(sigset_t));
}

int sigaddset(sigset_t *set, int signo) {
    if(signo <= 0 || signo > _NSIG)
        return EINVAL;

    set->set[(signo - 1) / 64] |= 1ul << ((signo - 1) & 63);
}

int sigdelset(sigset_t *set, int signo) {
    if(signo <= 0 || signo > _NSIG)
        return EINVAL;

    set->set[(signo - 1) / 64] &= ~(1ul << ((signo - 1) & 63));
}

int sigismember(const sigset_t *set, int signo) {
    return signo > 0 && signo <= _NSIG 
        && (set->set[(signo - 1) / 64] & 1ul << ((signo - 1) & 63)) != 0;
}
