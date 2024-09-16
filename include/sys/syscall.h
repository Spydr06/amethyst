#ifndef _AMETHYST_CPU_SYSCALLS_H
#define _AMETHYST_CPU_SYSCALLS_H

#include <cdefs.h>

#include <stdint.h>
#include <stddef.h>

#define __syscall __no_caller_saved_registers __general_regs_only

typedef struct {
    uint64_t ret;
    uint64_t _errno;
} syscallret_t;

typedef syscallret_t (*syscall_t)(void) __no_caller_saved_registers;

extern const size_t _syscall_count;

bool syscalls_init(void);

const char* _syscall_get_name(size_t i);

#endif /* _AMETHYST_CPU_SYSCALLS_H */

