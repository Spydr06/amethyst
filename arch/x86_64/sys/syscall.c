#include <sys/syscall.h>

#include <filesystem/virtual.h>

#include <x86_64/cpu/idt.h>
#include <x86_64/cpu/cpu.h>

#include <errno.h>
#include <cdefs.h>
#include <kernelio.h>

#define _SYS_E(_name, _fmt) [SYS_##_name] = ((struct syscall_entry){    \
        .syscall = (syscall_t) _sys_##_name,                            \
        .name = #_name,                                                 \
        .debug_fmt = _fmt                                               \
    })

struct syscall_entry {
    syscall_t syscall;
    const char* name;
    const char* debug_fmt;
};

const struct syscall_entry _syscall_table[] = {
    _SYS_E(read,     "%d, %p, %zu" ),
    _SYS_E(write,    "%d, %p, %zu" ),
    _SYS_E(open,     "%p, %lx, %lx"),
    _SYS_E(close,    "%d"          ),
    _SYS_E(stat,     "%p, %p"      ),
    _SYS_E(fstat,    "%d, %p"      ),
    _SYS_E(mkdir,    "%p, %lx"     ),
    _SYS_E(getdents, "%d, %p, %zu" ),
    _SYS_E(lseek,    "%d, %lx, %d" ),
    _SYS_E(mmap,     "%p, %zu, 0x%x, 0x%x, %d, %zu"),
    _SYS_E(munmap,   "%p, %zu"     ),
    _SYS_E(brk,      "%p"          ),
    _SYS_E(mount,    "%p, %p, %p, %lx, %p"),
    _SYS_E(umount,   "%p"          ),
    _SYS_E(ioctl,    "%d, %d, %p"  ),
    _SYS_E(access,   "%p, %x"      ),
    _SYS_E(yield,    ""            ),
    _SYS_E(nanosleep,"%p, %p"      ),
    _SYS_E(getpid,   ""            ),
    _SYS_E(fork,     ""            ),
    _SYS_E(execve,   "%p, %p, %p"  ),
    _SYS_E(exit,     "%ld"         ),
    _SYS_E(waitpid,  "%d, %p, %d, %p"),
    _SYS_E(uname,    "%p"          ),
    _SYS_E(getcwd,   "%p, %lx"     ),
    _SYS_E(sysinfo,  "%p"          ),
    _SYS_E(knldebug, "%ld, %p, %ld")
};

const size_t _syscall_count = __len(_syscall_table);

extern __syscall syscall_t _syscall_get_entry(size_t i) {
    return i < __len(_syscall_table) && _syscall_table[i].syscall ? _syscall_table[i].syscall : (syscall_t) _syscall_invalid;
}

const char* _syscall_get_name(size_t i) {
    return i < __len(_syscall_table) && _syscall_table[i].syscall ? _syscall_table[i].name : "invalid";
}

const char* _syscall_get_debug_fmt(size_t i) {
    return i < __len(_syscall_table) && _syscall_table[i].syscall ? _syscall_table[i].debug_fmt : "N/A";
}

bool syscalls_init(void)
{
    if(_cpu()->features.syscall_supported) {
        wrmsr(MSR_STAR,  (uint64_t) 0x13 << 48 | (uint64_t) 0x08 << 32);
        wrmsr(MSR_LSTAR, (uintptr_t) _syscall_entry);
        wrmsr(MSR_CSTAR, 0);
        wrmsr(MSR_FMASK, 0x200);
    }
    else {
        panic("No `syscall` support");
    }

    return true;
}

extern __syscall syscallret_t _syscall_invalid(struct cpu_context* ctx) {
    klog(ERROR, "Invalid syscall `%lu`.", ctx->rax);
    return (syscallret_t){
        ._errno = ENOSYS,
        .ret = 0
    };
}
