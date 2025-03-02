#include <io/poll.h>

#include <assert.h>
#include <cpu/interrupts.h>
#include <errno.h>
#include <mem/heap.h>
#include <sys/scheduler.h>
#include <sys/spinlock.h>
#include <sys/thread.h>

static void io_poll_enqueue(struct io_poll_queue** head, struct io_poll_queue* elem) {
    elem->prev = nullptr;
    elem->next = *head;
    
    if(*head)
        (*head)->prev = elem;
    *head = elem;
}

static void io_poll_dequeue(struct io_poll_queue** head, struct io_poll_queue* elem) {
    if(elem->prev)
        elem->prev->next = elem->next;

    if(elem->next)
        elem->next->prev = elem->prev;
    if(*head == elem)
        *head = elem->next;

    elem->prev = nullptr;
    elem->next = nullptr;
}

void io_poll_init(struct io_poll* poll) {
    spinlock_init(poll->lock);
}

void io_poll_event(struct io_poll* poll, enum io_poll_event events) {
    bool int_state = interrupt_set(false);
    spinlock_acquire(&poll->lock);

    struct io_poll_queue* pending = poll->head;
    poll->head = nullptr;

    struct io_poll_queue* iter = pending;

    while(iter) {
        struct io_poll_queue* next = iter->next;
        spinlock_acquire(&iter->event_lock);

        int handled = (iter->events | IO_POLL_HUP | IO_POLL_ERR) & events;
        if(handled == 0 || !spinlock_try(&iter->lock) || !spinlock_try(&iter->wakeup_lock)) {
            io_poll_dequeue(&poll->head, iter);
            io_poll_enqueue(&poll->head, iter);
        }

        spinlock_release(&iter->event_lock);
        iter = next;
    }

    iter = pending;
    while(iter) {
        struct io_poll_queue* next = iter->next;
        
        sched_wakeup(iter->thread, 0);
        if(iter->thread == _cpu()->thread)
            sched_yield();

        io_poll_enqueue(&poll->head, iter);
        spinlock_release(&iter->wakeup_lock);
        iter = next;
    }

    spinlock_release(&poll->lock);
    interrupt_set(int_state);
}

struct io_poll_queue* io_poll_make(enum io_poll_event events) {
    struct io_poll_queue* elem = kmalloc(sizeof(struct io_poll_queue));
    if(!elem)
        return nullptr;

    spinlock_init(elem->lock);
    spinlock_init(elem->event_lock);
    spinlock_init(elem->wakeup_lock);

    elem->thread = current_thread();
    elem->events = events;

    spinlock_init(elem->lock);
    return elem;
}

void io_poll_add(struct io_poll* poll, struct io_poll_queue* elem) {
    bool int_state = interrupt_set(false);
    spinlock_acquire(&poll->lock);

    io_poll_enqueue(&poll->head, elem);

    spinlock_release(&poll->lock);
    interrupt_set(int_state);
}

int io_poll_wait(struct io_poll_queue* poll) {
    bool int_state = interrupt_set(false);

    spinlock_acquire(&poll->event_lock);

    sched_prepare_sleep(true);
    
    spinlock_release(&poll->lock);
    spinlock_release(&poll->event_lock);

    int ret = sched_yield();
    assert(ret || !spinlock_try(&poll->lock));

    if(ret == WAKEUP_REASON_INTERRUPTED) {
        spinlock_acquire(&poll->wakeup_lock);
        ret = EINTR;
    }

    spinlock_release(&poll->lock);
    interrupt_set(int_state);

    return ret == 1 ? 0 : ret;
}

void io_poll_remove(struct io_poll* poll, struct io_poll_queue* elem) {
    bool int_state = interrupt_set(false);
    spinlock_acquire(&elem->lock);
    
    io_poll_dequeue(&poll->head, elem);

    spinlock_release(&elem->lock);
    interrupt_set(int_state);
}

void io_poll_free_queue(struct io_poll_queue* poll) {
    kfree(poll);
}

