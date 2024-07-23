#ifndef _AMETHYST_SYS_TIMER_H
#define _AMETHYST_SYS_TIMER_H

#include <sys/spinlock.h>
#include <sys/dpc.h>

#include <time.h>

struct timer {
    spinlock_t lock;

    time_t ticks_per_us;
    time_t current_tick;

    bool running;

    void (*arm)(time_t);
    time_t (*stop)(void);
};

struct timer_entry {
    struct timer_entry* next;
    time_t absolute_tick;
    time_t repeat_us;
};

struct timer* timer_init(time_t ticks_per_us, void(*arm)(time_t), time_t (*stop)(void));

void timer_insert(struct timer* timer, struct timer_entry* entry, dpc_fn_t callback, dpc_arg_t arg, time_t us, bool repeating);

#endif /* _AMETHYST_SYS_TIMER_H */

