#include <sys/timer.h>

#include <mem/heap.h>

struct timer* timer_init(time_t ticks_per_us, void(*arm)(time_t), time_t (*stop)(void)) {
    struct timer* timer = kmalloc(sizeof(struct timer));
    if(!timer)
        return NULL;

    timer->ticks_per_us = ticks_per_us;
    timer->running = false;
    timer->arm = arm;
    timer->stop = stop;

    spinlock_init(timer->lock);

    return timer;
}

