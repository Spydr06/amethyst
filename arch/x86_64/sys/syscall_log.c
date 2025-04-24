#include <sys/syscall_log.h>
#include <sys/syscall.h>
#include <sys/thread.h>
#include <sys/proc.h>

#include <cpu/cpu.h>

#include <assert.h>
#include <kernelio.h>
#include <string.h>

bool _syscall_log_enabled = false;

void syscall_log_set(bool enabled) {
    __atomic_store(&_syscall_log_enabled, &enabled, __ATOMIC_SEQ_CST);
}

bool syscall_log_enabled(void) {
    return _syscall_log_enabled;
}

extern __syscall void _syscall_log(register_t syscall, register_t a1, register_t a2, register_t a3, register_t a4, register_t a5, register_t a6) {
    if(_syscall_log_enabled) {
        bool before = interrupt_set(false);

        char fmt[1024] = "\e[32m[%04d %04d] >>>\e[0m _sys_%s(";
        strcat(fmt, _syscall_get_debug_fmt(syscall));
        strcat(fmt, ")");

        struct thread* thread = current_thread();
        assert(thread);

        klog(DEBUG, fmt, thread->proc->pid, thread->tid, _syscall_get_name(syscall), a1, a2, a3, a4, a5, a6);
        
        interrupt_set(before);
    }
}

extern __syscall void _syscall_log_return(uint64_t ret, uint64_t _errno) {
    if(_syscall_log_enabled) {
        bool before = interrupt_set(false);

        struct thread* thread = current_thread();
        assert(thread);

        klog(DEBUG, "\e[32m[%04d %04d] <<<\e[0m sysret: %ld (%s)", thread->proc->pid, thread->tid, ret, strerror(_errno));

        interrupt_set(before);
    }
}

