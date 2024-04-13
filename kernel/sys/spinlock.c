#include <sys/spinlock.h>

void spinlock_acquire(spinlock_t* lock) {
    while(__atomic_test_and_set(&lock->locked, __ATOMIC_ACQUIRE));
}

void spinlock_release(spinlock_t* lock) {
    __atomic_clear(&lock->locked, __ATOMIC_RELEASE);
}


