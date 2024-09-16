#include <sys/syscall.h>
#include <sys/thread.h>
#include <cpu/cpu.h>

#ifdef _AMETHYST_LOG_SYSCALLS
#include "kernelio.h"
#include <string.h>
#include <sys/proc.h>

static const char* syscall_dbg_fmt[] = {
    "(%ld %ld %ld %ld %ld %ld)"
};

static const char invalid_syscall_fmt[] = " N/A";
#endif

extern __syscall void _syscall_log(register_t syscall, register_t a1, register_t a2, register_t a3, register_t a4, register_t a5, register_t a6) {
#ifdef _AMETHYST_LOG_SYSCALLS
    char fmt[1024] = "\e[32m[%04d %04d] >>>\e[0m _sys_%s";
    
    const char* specific_fmt = syscall < _syscall_count ? syscall_dbg_fmt[syscall] : invalid_syscall_fmt;
    strcat(fmt, specific_fmt);

    struct proc* proc = _cpu()->thread->proc;
    klog(DEBUG, fmt, proc ? proc->pid : -1, _cpu()->thread->tid, _syscall_get_name(syscall), a1, a2, a3, a4, a5, a6);
#else
    (void) syscall;
    (void) a1;
    (void) a2;
    (void) a3;
    (void) a4;
    (void) a5;
    (void) a6;
#endif
}

extern __syscall void _syscall_log_return(uint64_t ret, uint64_t _errno) {
#ifdef _AMETHYST_LOG_SYSCALLS
    struct proc* proc = _cpu()->thread->proc;
    klog(DEBUG, "\e[32m[%04d %04d] <<<\e[0m sysret: %lu (%s)", proc ? proc->pid : -1, _cpu()->thread->tid, ret, strerror(_errno));
#else
    (void) ret;
    (void) _errno;
#endif
}

