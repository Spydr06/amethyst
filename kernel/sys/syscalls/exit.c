#include <sys/syscall.h>
#include <sys/scheduler.h>

#include <cdefs.h>

__syscall syscallret_t _sys_exit(struct cpu_context* ctx, int exit_code) {
    scheduler_terminate(exit_code << 8);
    unreachable();
}

