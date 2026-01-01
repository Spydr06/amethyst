#include "mem/user.h"
#include "x86_64/cpu/msr.h"
#include <sys/syscall.h>

#include <amethyst/prctl.h>
#include <errno.h>

union prctl_arg {
    unsigned long addr;
    unsigned long *addr_ptr;
};

__syscall syscallret_t _sys_arch_prctl(struct cpu_context *ctx, int op, union prctl_arg arg) {
    syscallret_t ret = {
        .ret = 0,
        ._errno = 0
    };

    switch(op) {
        case ARCH_SET_FS:
            wrmsr(MSR_FSBASE, arg.addr);
            break;
        case ARCH_GET_FS:
            if(!arg.addr_ptr)
                ret._errno = EINVAL;
            else {
                unsigned long addr = rdmsr(MSR_FSBASE);
                ret._errno = memcpy_to_user(arg.addr_ptr, &addr, sizeof(unsigned long));
            }
            break;
        default:
            ret._errno = EINVAL;
    }

    return ret;
}

_SYSCALL_REGISTER(SYS_arch_prctl, _sys_arch_prctl, "arch_prctl", "%x, %p");
