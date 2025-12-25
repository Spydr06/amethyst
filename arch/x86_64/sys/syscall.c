#include <sys/syscall.h>
#include <sys/spinlock.h>
#include <amethyst/syscall.h>

#include <filesystem/vfs.h>

#include <x86_64/cpu/idt.h>
#include <x86_64/cpu/cpu.h>

#include <assert.h>
#include <cdefs.h>
#include <errno.h>
#include <kernelio.h>
#include <memory.h>

extern const struct syscall_entry _STATIC_SYSCALLS_START_[];
extern const struct syscall_entry _STATIC_SYSCALLS_END_[];

static spinlock_t syscall_table_lock;
static bool syscall_table_initialized;
static struct syscall_entry syscall_table[SYS_MAXIMUM];

static_assert(_Alignof(struct syscall_entry) == sizeof(syscall_t));

extern __syscall syscall_t _syscall_get_entry(size_t i) {
    return i < __len(syscall_table) && syscall_table[i].syscall ? syscall_table[i].syscall : (syscall_t) _syscall_invalid;
}

const char* _syscall_get_name(size_t i) {
    return i < __len(syscall_table) && syscall_table[i].syscall ? syscall_table[i].name : "invalid";
}

const char* _syscall_get_debug_fmt(size_t i) {
    return i < __len(syscall_table) && syscall_table[i].syscall ? syscall_table[i].debug_fmt : "N/A";
}

static void populate_syscall_table(void) {
    if(!__sync_bool_compare_and_swap(&syscall_table_initialized, false, true))
        return;

    spinlock_init(syscall_table_lock);
    spinlock_acquire(&syscall_table_lock);

    const struct syscall_entry* static_cur = _STATIC_SYSCALLS_START_;
    const struct syscall_entry* static_end = _STATIC_SYSCALLS_END_;

    size_t i;
    for(i = 0; static_cur < static_end; static_cur++, i++) {
        assert(static_cur->syscall && "invalid syscall_entry in .static_syscalls section");
        syscallnum_t number = static_cur->number;

        assert(number < SYS_MAXIMUM && "syscall entry has invalid number");

        struct syscall_entry *entry = syscall_table + number;
        if(entry->syscall) {
            spinlock_release(&syscall_table_lock);
            panic("Duplicate syscall `%u`: Tried to register `%s`, but `%s` already exists.",
                number, static_cur->name, entry->name);
        }

        memcpy(entry, static_cur, sizeof(struct syscall_entry));
    }

    spinlock_release(&syscall_table_lock);

    klog(DEBUG, "Registered %zu system calls.", i);
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

    populate_syscall_table();

    return true;
}

int syscall_register(const struct syscall_entry *entry) {
    if(!entry)
        return EINVAL;

    assert(syscall_table_initialized);

    int err = 0;
    spinlock_acquire(&syscall_table_lock); 

    if(entry->number >= SYS_MAXIMUM) {
        err = ERANGE;
        goto cleanup;
    }

    if(syscall_table[entry->number].syscall) {
        err = EEXIST;
        goto cleanup;
    }

    memcpy(syscall_table + entry->number, entry, sizeof(struct syscall_entry));

cleanup:
    spinlock_release(&syscall_table_lock);
    return err;
}

const struct syscall_entry *syscall_get(syscallnum_t number) {
    assert(syscall_table_initialized);

    if(number >= SYS_MAXIMUM)
        return NULL;

    return syscall_table + number;
}

extern __syscall syscallret_t _syscall_invalid(struct cpu_context* ctx) {
    klog(ERROR, "Invalid syscall `%lu`.", ctx->rax);
    return (syscallret_t){
        ._errno = ENOSYS,
        .ret = 0
    };
}

