#ifndef _AMETHYST_SYS_SPINLOCK_H
#define _AMETHYST_SYS_SPINLOCK_H

typedef volatile bool spinlock_t;

#define spinlock_init(x) ((x) = 0)

static inline void spinlock_acquire(spinlock_t* lock) {
    while(!__sync_bool_compare_and_swap(lock, false, true))
        __asm__ volatile ("pause");
}

static inline bool spinlock_try(spinlock_t* lock) {
    return __sync_bool_compare_and_swap(lock, false, true);
}

static inline void spinlock_release(spinlock_t* lock) {
    *lock = false;
}

#endif /* _AMETHYST_SYS_SPINLOCK_H */

