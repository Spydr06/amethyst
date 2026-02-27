#include <signal.h>
#include <unistd.h>

#include <bits/alltypes.h>

#include <sys/syscall.h>
#include <internal/syscall.h>

int kill(pid_t pid, int sig) {
    return syscall(SYS_kill, pid, sig);
}

int raise(int sig) {
    // TODO: fix for multithreaded programs
    return kill(getpid(), sig);
}

