#include <sys/syscall.h>

__syscall syscallret_t _sys_getuid(struct cpu_context *__unused) {
    return (syscallret_t) {
        ._errno = 0,
        .ret = 0, // FIXME: when implementing users
    };
}

__syscall syscallret_t _sys_geteuid(struct cpu_context *__unused) {
    return (syscallret_t) {
        ._errno = 0,
        .ret = 0, // FIXME: when implementing users
    };
}


__syscall syscallret_t _sys_getgid(struct cpu_context *__unused) {
    return (syscallret_t) {
        ._errno = 0,
        .ret = 0, // FIXME: when implementing users
    };
}

__syscall syscallret_t _sys_getegid(struct cpu_context *__unused) {
    return (syscallret_t) {
        ._errno = 0,
        .ret = 0, // FIXME: when implementing users
    };
}

_SYSCALL_REGISTER(SYS_getuid, _sys_getuid, "getuid", "");
_SYSCALL_REGISTER(SYS_geteuid, _sys_geteuid, "geteuid", "");
_SYSCALL_REGISTER(SYS_getgid, _sys_getgid, "getgid", "");
_SYSCALL_REGISTER(SYS_getegid, _sys_getegid, "getegid", "");

