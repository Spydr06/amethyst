#include <sys/semaphore.h>
#include <cpu/interrupts.h>
#include <cpu/cpu.h>

#include <kernelio.h>

int semaphore_wait(semaphore_t *sem, bool interruptible __attribute__((unused))) {
/*    if(!_cpu()->thread) {
        while(!semaphore_test(sem))
            pause();
        return 0;
    }*/

//    unimplemented();
    return 0;
}

bool semaphore_test(semaphore_t *sem) {
    bool int_state = interrupt_set(false);
    bool ret = false;

    spinlock_acquire(&sem->lock);

    if(sem->i > 0) {
        sem->i--;
        ret = true;
    }

    spinlock_release(&sem->lock);
    interrupt_set(int_state);

    return ret;
}

void semaphore_signal(semaphore_t *sem) {
    bool int_state = interrupt_set(false);
    spinlock_acquire(&sem->lock);

//    if(++sem->i <= 0) {
// TODO:
//    }

    spinlock_release(&sem->lock);
    interrupt_set(int_state);
}

