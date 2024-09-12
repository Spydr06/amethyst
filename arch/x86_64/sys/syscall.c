#include <sys/syscall.h>

#include <filesystem/virtual.h>

#include <x86_64/cpu/idt.h>
#include <x86_64/cpu/cpu.h>

#include <errno.h>
#include <cdefs.h>
#include <kernelio.h>

#define _SYS_E(_name) ((struct syscall_entry){ \
        .syscall = _sys_##_name,               \
        .name = #_name,                        \
    })

struct syscall_entry {
    syscall_t syscall;
    const char* name;
};

extern void _syscall_entry(void);
extern __syscall syscallret_t _syscall_invalid(void);
extern __syscall syscallret_t _sys_test(void);

const struct syscall_entry _syscall_table[] = {
    _SYS_E(test)    
};

extern __syscall syscall_t _syscall_get_entry(size_t i) {
    return i < __len(_syscall_table) ? _syscall_table[i].syscall : _syscall_invalid;
}

const char* _syscall_get_name(size_t i) {
    return i < __len(_syscall_table) ? _syscall_table[i].name : "invalid";
}

bool syscalls_init(void)
{
    if(_cpu()->features.syscall_supported) {
        uint64_t star = 0x13ul << 48 | 0x08ul << 32;
        wrmsr(MSR_STAR, star);
        wrmsr(MSR_LSTAR, (uintptr_t) _syscall_entry);
        wrmsr(MSR_CSTAR, 0);
        wrmsr(MSR_FMASK, 0x200);
    }
    else {
        panic("No `syscall` support");
    }

    return true;
}

extern __syscall syscallret_t _sys_test(void) {
    klog(WARN, "in syscall `test`!");
    return (syscallret_t){
        ._errno = 0,
        .ret = 0
    };
}

extern __syscall syscallret_t _syscall_invalid(void) {
    return (syscallret_t){
        ._errno = ENOSYS,
        .ret = 0
    };
}
