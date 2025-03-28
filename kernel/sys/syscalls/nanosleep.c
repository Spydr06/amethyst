#include "sys/thread.h"
#include <sys/syscall.h>

#include <errno.h>
#include <mem/user.h>
#include <sys/dpc.h>
#include <sys/scheduler.h>
#include <sys/timer.h>
#include <time.h>

static void timeout(struct cpu_context*, dpc_arg_t arg) {
    sched_wakeup((struct thread*) arg, 0);
}

static __attribute__((noinline)) struct timer* _current_timer(void) {
    return _cpu()->timer;
}

static __attribute__((noinline)) int _current_cpuid(void) {
    return _cpu()->id;
}

__syscall syscallret_t _sys_nanosleep(struct cpu_context*, const struct timespec* rq_user, struct timespec* rm_user) {
    syscallret_t ret = {
        .ret = 0,
    };

    struct timespec rq;
    ret._errno = memcpy_from_user(&rq, rq_user, sizeof(struct timespec));
    if(ret._errno)
        return ret;

    if(rq.ns < 0 || rq.s < 0) {
        ret._errno = EINVAL;
        return ret;
    }

    struct timer_entry timer;
    sched_prepare_sleep(true);

    sched_pin(_current_cpuid());
    timer_insert(_current_timer(), &timer, timeout, current_thread(), rq.s * 1'000'000 + rq.ns / 1'000, false);
    
    if(sched_yield() == WAKEUP_REASON_INTERRUPTED)
        ret._errno = EINTR;
    
    if(ret._errno && rm_user) {
        uintmax_t remaining_us = timer_remove(_current_timer(), &timer);
        struct timespec rm = {
            .ns = (remaining_us % 1'000'000) * 1'000,
            .s = (remaining_us / 1'000'000)
        };
        if(memcpy_to_user(rm_user, &rm, sizeof(struct timespec)))
            ret._errno = EFAULT;
    }

    sched_pin(THREAD_UNPINNED);

    ret.ret = 0;
    return ret;
}

