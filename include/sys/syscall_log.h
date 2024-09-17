#ifndef _AMETHYST_SYS_SYSCALL_LOG_H
#define _AMETHYST_SYS_SYSCALL_LOG_H

#include <cdefs.h>

void syscall_log_set(bool enabled);
bool syscall_log_enabled(void);

static __always_inline void syscall_log_enable(void) {
    syscall_log_set(true); 
}

static __always_inline void syscall_log_disable(void) {
    syscall_log_set(false);
}

#endif /* _AMETHYST_SYS_SYSCALL_LOG_H */

