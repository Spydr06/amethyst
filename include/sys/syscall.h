#ifndef _AMETHYST_CPU_SYSCALLS_H
#define _AMETHYST_CPU_SYSCALLS_H

#include <amethyst/dirent.h>
#include <amethyst/stat.h>
#include <amethyst/syscall.h>
#include <amethyst/syscall.h>
#include <amethyst/sysinfo.h>
#include <amethyst/utsname.h>

#include <cdefs.h>
#include <cpu/cpu.h>
#include <kernelio.h>
#include <sys/mmap.h>
#include <amethyst/timeval.h>

#include <stdint.h>
#include <stddef.h>

#define __syscall __general_regs_only __no_caller_saved_registers

typedef struct {
    uint64_t ret;
    uint64_t _errno;
} syscallret_t;

typedef syscallret_t (*syscall_t)(...) __no_caller_saved_registers;

typedef uint32_t syscallnum_t;

struct syscall_entry {
    syscallnum_t number;
    syscall_t syscall;
    const char* name;
    const char* debug_fmt;
};

#define _SYSCALL_REGISTER(_num, _func, _name, _debug)                                                               \
    static_assert((_num) >= 0 && (_num) < SYS_MAXIMUM);                                                            \
    __attribute__((section(".static_syscalls"), used)) alignas(syscall_t) struct syscall_entry __sys_ent_##_func    \
        = { .number = (_num), .syscall = (syscall_t)(_func), .name = (_name), .debug_fmt = (_debug) }

bool syscalls_init(void);

int syscall_register(const struct syscall_entry *entry);
const struct syscall_entry *syscall_get(syscallnum_t number);

const char* _syscall_get_name(size_t i);
const char* _syscall_get_debug_fmt(size_t i);

extern void _syscall_entry(void);

__syscall syscallret_t _syscall_invalid(struct cpu_context* ctx);

#endif /* _AMETHYST_CPU_SYSCALLS_H */

