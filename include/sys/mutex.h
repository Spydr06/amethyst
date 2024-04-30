#ifndef _AMETHYST_SYS_MUTEX_H
#define _AMETHYST_SYS_MUTEX_H

#include "semaphore.h"

typedef semaphore_t mutex_t;

#define mutex_init(m) semaphore_init(m, 1)
#define mutex_acquire(m, i) (semaphore_wait((m), (i)))
#define mutex_release(m) (semaphore_signal(m))
#define mutex_try(m) (semaphore_test(m))

#endif /* _AMETHYST_SYS_MUTEX_H */

