#include "sys/thread.h"
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
#include <string.h>

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

    int err = syscall_register_section(_STATIC_SYSCALLS_START_, _STATIC_SYSCALLS_END_);
    if(err) {
        panic("Could not register syscalls: %s", strerror(err));
    }
}

int syscall_register_section(const void *start, const void *end) {
    for(; start < end; start += sizeof(struct syscall_entry)) {
        int err = syscall_register(start);
        if(err)
            return err;
    }

    return 0;
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

