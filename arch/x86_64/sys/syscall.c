#include <sys/syscall.h>

#include <x86_64/cpu/idt.h>
#include <x86_64/cpu/cpu.h>

#include <kernelio.h>

extern void _syscall_entry(void);

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

extern void _sys_test(void) {
}

extern void _syscall_invalid(void) {
}
