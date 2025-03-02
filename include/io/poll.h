#ifndef _AMETHYST_IO_POLL_H
#define _AMETHYST_IO_POLL_H

#include <stddef.h>
#include <stdint.h>
#include <sys/spinlock.h>

enum io_poll_event : uint8_t {
    IO_POLL_ERR = 0x01,
    IO_POLL_HUP = 0x02,
    IO_POLL_IN  = 0x04,
    IO_POLL_OUT = 0x08
};

struct io_poll_queue {
    spinlock_t lock;
    spinlock_t event_lock;
    spinlock_t wakeup_lock;
    enum io_poll_event events;

    struct io_poll_queue* next;
    struct io_poll_queue* prev;
    
    struct thread* thread;
};

struct io_poll {
    spinlock_t lock;
    struct io_poll_queue* head;
};

void io_poll_init(struct io_poll* poll);
void io_poll_event(struct io_poll* poll, enum io_poll_event event);

struct io_poll_queue* io_poll_make(enum io_poll_event events);
void io_poll_add(struct io_poll* poll, struct io_poll_queue* elem);
void io_poll_remove(struct io_poll* poll, struct io_poll_queue* elem);

// TODO: implement timeouts
int io_poll_wait(struct io_poll_queue* poll);

void io_poll_free_queue(struct io_poll_queue* poll);

#endif /* _AMETHYST_IO_POLL_H */

