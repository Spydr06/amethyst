#include <sys/syscall.h>

#include <kernelio.h>

__syscall syscallret_t _sys_waitpid(struct cpu_context* ctx, pid_t pid, int *wstatus, int options) {
    unimplemented();
}

