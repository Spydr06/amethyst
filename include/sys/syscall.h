#ifndef _AMETHYST_CPU_SYSCALLS_H
#define _AMETHYST_CPU_SYSCALLS_H

#include <cdefs.h>
#include <cpu/cpu.h>
#include <kernelio.h>

#include <stdint.h>
#include <stddef.h>

#define __syscall __no_caller_saved_registers __general_regs_only

enum syscall {
    _SYS_exit     = 60,
    _SYS_knldebug = 255
};

typedef struct {
    uint64_t ret;
    uint64_t _errno;
} syscallret_t;

typedef syscallret_t (*syscall_t)(...) __no_caller_saved_registers;

extern const size_t _syscall_count;

bool syscalls_init(void);

const char* _syscall_get_name(size_t i);
const char* _syscall_get_debug_fmt(size_t i);

extern void _syscall_entry(void);

__syscall syscallret_t _syscall_invalid(struct cpu_context* ctx);

// actual syscalls located in `kernel/sys/syscalls/`

__syscall syscallret_t _sys_exit(struct cpu_context* ctx, int exit_code);
__syscall syscallret_t _sys_knldebug(struct cpu_context* ctx, enum klog_severity severity, const char* user_buffer, size_t buffer_size);

#endif /* _AMETHYST_CPU_SYSCALLS_H */

