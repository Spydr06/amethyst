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
    if(!set) {
        errno = EINVAL;
        return -1;
    }

    memset(set, 0, sizeof(sigset_t));
    return 0;
}

int sigfillset(sigset_t *set) {
    if(!set) {
        errno = EINVAL;
        return -1;
    }

    memset(set, 0xff, sizeof(sigset_t));
    return 0;
}

int sigaddset(sigset_t *set, int signo) {
    if(!set || signo <= 0 || signo > _NSIG) {
        errno = EINVAL;
        return -1;
    }

    set->set[(signo - 1) / 64] |= 1ul << ((signo - 1) & 63);
    return 0;
}

int sigdelset(sigset_t *set, int signo) {
    if(!set || signo <= 0 || signo > _NSIG) {
        errno = EINVAL;
        return -1;
    }

    set->set[(signo - 1) / 64] &= ~(1ul << ((signo - 1) & 63));
    return 0;
}

int sigismember(const sigset_t *set, int signo) {
    if(!set || signo <= 0 || signo > _NSIG) {
        errno = EINVAL;
        return -1;
    }

    return (set->set[(signo - 1) / 64] & 1ul << ((signo - 1) & 63)) != 0;
}
