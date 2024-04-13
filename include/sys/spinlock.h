#ifndef _AMETHYST_SYS_SPINLOCK_H
#define _AMETHYST_SYS_SPINLOCK_H

typedef struct {
    bool locked;
} spinlock_t;

void spinlock_acquire(spinlock_t* lock);
void spinlock_release(spinlock_t* lock);

#endif /* _AMETHYST_SYS_SPINLOCK_H */

