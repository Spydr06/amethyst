#include <signal.h>
#include <unistd.h>

#include <bits/alltypes.h>

#include <amethyst/syscall.h>
#include <internal/syscall.h>

#include <memory.h>

int sigaction(int sig, const struct sigaction *restrict act, struct sigaction *restrict oldact) {
    return syscall(SYS_sigaction, sig, act, oldact);
}

void (*signal(int sig, void (*func)(int)))(int) {
    struct sigaction action;
    if(sigaction(sig, NULL, &action))
        return SIG_ERR; // errno set
    
    void (*oldfunc)(int) = action.sa_handler;
    action.sa_handler = func;

    if(sigaction(sig, &action, NULL))
        return SIG_ERR; // errno set

    return oldfunc;
}

