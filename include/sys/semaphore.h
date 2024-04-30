#ifndef _AMETHYST_SYS_SEMAPHORE_H
#define _AMETHYST_SYS_SEMAPHORE_H

#include "spinlock.h"

#define SEM_TAIL 0
#define SEM_HEAD 1

typedef struct semaphore {
    int i;
    spinlock_t lock;
    struct thread* tail;
    struct thread* head;
} semaphore_t;

#define semaphore_init(x, v) do {   \
        (x)->i = (v);               \
        spinlock_init((x)->lock);   \
        (x)->tail = nullptr;        \
        (x)->head = nullptr;        \
    } while(0)

// TODO: once threading is implemented:
int semaphore_wait(semaphore_t *sem, bool interruptible);
void semaphore_signal(semaphore_t *sem);
bool semaphore_test(semaphore_t *sem);
bool semaphore_haswaiters(semaphore_t *sem);

#endif /* _AMETHYST_SYS_SEMAPHORE_H */

