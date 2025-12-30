#include <sys/syscall.h>
#include <sys/scheduler.h>

#include <cdefs.h>

__syscall syscallret_t _sys_exit(struct cpu_context* __unused, int exit_code) {
    scheduler_terminate(exit_code << 8);
    unreachable();
}

_SYSCALL_REGISTER(SYS_exit, _sys_exit, "exit", "%ld");
