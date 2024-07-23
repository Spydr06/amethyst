#include <sys/timer.h>
#include <sys/dpc.h>
#include <sys/spinlock.h>

#include <cpu/cpu.h>

#include <mem/heap.h>

#include <kernelio.h>
#include <assert.h>

static void enqueue(struct timer* timer, struct timer_entry* entry, time_t us) {
    entry->absolute_tick = timer->current_tick + us * timer->ticks_per_us;

    if(!timer->queue) {
        entry->next = nullptr;
        timer->queue = entry;
    }
    else if(timer->queue->absolute_tick > entry->absolute_tick) {
        assert(!entry->next);
        entry->next = timer->queue;
        timer->queue = entry;
    }
    else {
        struct timer_entry* search = timer->queue;

        while(search) {
            assert(search != entry);
            if(!search->next) {
                entry->next = nullptr;
                search->next = entry;
                break;
            }
            else if(entry->absolute_tick >= search->absolute_tick && entry->absolute_tick < search->next->absolute_tick) {
                entry->next = search->next;
                search->next = entry;
                break;
            }

            search = search->next;
        }
    }
}

static void timer_check(struct timer* timer) {
    while(timer->queue && timer->queue->absolute_tick == timer->current_tick) {
        struct timer_entry* entry = timer->queue;
        timer->queue = timer->queue->next;
        entry->next = nullptr;

        if(entry->repeat_us)
            enqueue(timer, entry, entry->repeat_us);
        else
            entry->fired = true;

        dpc_enqueue(&entry->dpc, entry->callback, entry->arg);
    }
}

void timer_resume(struct timer* timer) {
    enum ipl old_ipl = interrupt_raise_ipl(IPL_TIMER);
    spinlock_acquire(&timer->lock);

    timer->running = true;

    if(timer->queue) {
        timer_check(timer);
        timer->arm(timer->queue->absolute_tick - timer->current_tick);
    }

    spinlock_release(&timer->lock);
    interrupt_lower_ipl(old_ipl);
}

void timer_stop(struct timer* timer) {
    enum ipl old_ipl = interrupt_raise_ipl(IPL_TIMER);
    spinlock_acquire(&timer->lock);

    timer->current_tick += timer->stop(/* timer */);
    timer->running = false;

    spinlock_release(&timer->lock);
    interrupt_lower_ipl(old_ipl);
}

struct timer* timer_init(time_t ticks_per_us, void(*arm)(time_t), time_t (*stop)(void)) {
    struct timer* timer = kmalloc(sizeof(struct timer));
    if(!timer)
        return NULL;

    timer->ticks_per_us = ticks_per_us;
    timer->running = false;
    timer->arm = arm;
    timer->stop = stop;
    timer->queue = nullptr;

    spinlock_init(timer->lock);

    return timer;
}

uintmax_t timer_remove(struct timer* timer, struct timer_entry* entry) {
    (void) timer;
    (void) entry;
    unimplemented();
}

void timer_insert(struct timer* timer, struct timer_entry* entry, dpc_fn_t callback, dpc_arg_t arg, time_t us, bool repeating) {
    enum ipl old_ipl = interrupt_raise_ipl(IPL_TIMER);
    spinlock_acquire(&timer->lock);

    if(timer->running)
        timer->current_tick += timer->stop(/* timer */);

    memset(entry, 0, sizeof(struct timer_entry));
    entry->repeat_us = repeating ? us : 0;
    entry->callback = callback;
    entry->arg = arg;
    entry->fired = false;

    enqueue(timer, entry, us);

    if(timer->running) {
        timer_check(timer);
        timer->arm(timer->queue->absolute_tick - timer->current_tick);
    }

    spinlock_release(&timer->lock);
    interrupt_lower_ipl(old_ipl);
}

void timer_isr(struct timer* timer, struct cpu_context* context __unused) {
    spinlock_acquire(&timer->lock);

    assert(timer->queue);

    timer->current_tick = timer->queue->absolute_tick;

    struct timer_entry* old_entry = timer->queue;
    timer->queue = timer->queue->next;
    old_entry->next = nullptr;

    if(old_entry->repeat_us)
        enqueue(timer, old_entry, old_entry->repeat_us);
    else
        old_entry->fired = true;

    dpc_enqueue(&old_entry->dpc, old_entry->callback, old_entry->arg);

    if(timer->queue) {
        timer_check(timer);
        timer->arm(timer->queue->absolute_tick - timer->current_tick);
    }

    spinlock_release(&timer->lock);
}

