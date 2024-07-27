#ifndef _AMETHYST_SCHEDULER_THREAD_H
#define _AMETHYST_SCHEDULER_THREAD_H

#include <abi.h>
#include <signal.h>

#include <sys/spinlock.h>
#include <cpu/cpu.h>

enum thread_flags {
    THREAD_FLAGS_QUEUED = 1,
    THREAD_FLAGS_RUNNING = 2,
    THREAD_FLAGS_SLEEP = 4,
    THREAD_FLAGS_INTERRUPTIBLE = 8,
    THREAD_FLAGS_PREEMPTED = 16,
    THREAD_FLAGS_DEAD = 32
};

struct thread {
    void* kernel_stack_top;
    void* kernel_stack;

    struct vmm_context* vmm_context;
    struct proc* proc;

    enum thread_flags flags;
    int priority;

    tid_t tid;

    spinlock_t sleep_lock;

    struct cpu_context context;
    struct cpu_extra_context extra_context;

    struct {
        spinlock_t lock;
        struct stack stack;
        struct sigset mask;
        struct sigset pending;
        struct sigset urgent;
        bool stopped;
    } signals;
};

#endif /* _AMETHYST_SCHEDULER_THREAD_H */

