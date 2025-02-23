#include <sys/syscall.h>
#include <sys/scheduler.h>

syscallret_t _sys_yield(struct cpu_context* __unused) {
    sched_yield();
    return (syscallret_t) {.ret = 0, ._errno = 0};
}

