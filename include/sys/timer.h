#ifndef _AMETHYST_SYS_TIMER_H
#define _AMETHYST_SYS_TIMER_H

#include <sys/spinlock.h>

#include <time.h>

struct timer {
    spinlock_t lock;

    time_t ticks_per_us;
    time_t current_tick;

    bool running;

    void (*arm)(time_t);
    time_t (*stop)(void);
};

struct timer* timer_init(time_t ticks_per_us, void(*arm)(time_t), time_t (*stop)(void));

#endif /* _AMETHYST_SYS_TIMER_H */

