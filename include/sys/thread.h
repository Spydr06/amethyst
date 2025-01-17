#ifndef _AMETHYST_SCHEDULER_THREAD_H
#define _AMETHYST_SCHEDULER_THREAD_H

#include <abi.h>
#include <signal.h>

#include <sys/spinlock.h>
#include <cpu/cpu.h>
#include <mem/vmm.h>

#define THREAD_UNPINNED ((cpuid_t) -1)

enum thread_flags {
    THREAD_FLAGS_QUEUED = 1,
    THREAD_FLAGS_RUNNING = 2,
    THREAD_FLAGS_SLEEP = 4,
    THREAD_FLAGS_INTERRUPTIBLE = 8,
    THREAD_FLAGS_PREEMPTED = 16,
    THREAD_FLAGS_DEAD = 32
};

enum wakeup_reason {
    WAKEUP_REASON_NORMAL = 0,
    WAKEUP_REASON_INTERRUPTED = -1
};

struct thread {
    void* kernel_stack_top;
    void* kernel_stack;

    // queue for the scheduler
    struct thread* prev;
    struct thread* next;

    // queue for semaphores
    struct thread* sleep_prev;
    struct thread* sleep_next;

    struct vmm_context* vmm_context;
    struct proc* proc;
    void* user_break_base;
    void* user_break;

    enum thread_flags flags;
    int priority;

    tid_t tid;

    spinlock_t sleep_lock;
    enum wakeup_reason wakeup_reason;
    bool sleep_interrupt_status;

    bool should_exit;

    struct cpu* cpu;
    cpuid_t pin;

    int _errno;

    struct cpu_context context;
    struct cpu_extra_context extra_context;

    struct cpu_context* user_memcpy_context;

    struct {
        spinlock_t lock;
        struct stack stack;
        struct sigset mask;
        struct sigset pending;
        struct sigset urgent;
        bool stopped;
    } signals;
};

static_assert(sizeof(struct thread) <= PAGE_SIZE);

#endif /* _AMETHYST_SCHEDULER_THREAD_H */

