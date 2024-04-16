#ifndef _AMETHYST_CPU_SYSCALLS_H
#define _AMETHYST_CPU_SYSCALLS_H

#include <cpu/cpu.h>
#include <stddef.h>

#define E_NO_SYSCALL (-1)

bool syscalls_init(void);
cpu_status_t* syscall_dispatch(cpu_status_t* regs);

size_t execute_syscall(size_t syscall_num, size_t arg0, size_t arg1, size_t arg2);

#endif /* _AMETHYST_CPU_SYSCALLS_H */

