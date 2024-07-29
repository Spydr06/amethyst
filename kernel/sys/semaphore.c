#include <sys/semaphore.h>
#include <sys/scheduler.h>
#include <cpu/interrupts.h>
#include <cpu/cpu.h>

#include <assert.h>
#include <kernelio.h>

static void insert_thread(semaphore_t* sem, struct thread* thread) {
    thread->sleep_next = sem->head;
    thread->sleep_prev = nullptr;

    sem->head = thread;

    if(!sem->tail)
        sem->tail = thread;

    if(thread->sleep_next)
        thread->sleep_next->sleep_prev = thread;
}

static struct thread* next_thread(semaphore_t* sem) {
    struct thread* thread = sem->tail;
    if(!thread)
        return NULL;

    sem->tail = thread->sleep_prev;

    if(sem->tail)
        sem->tail->sleep_next = nullptr;
    else
        sem->head = nullptr;

    thread->sleep_prev = nullptr;
    thread->sleep_next = nullptr;

    return thread;
}

static void remove_current_thread(semaphore_t* sem) {
    struct thread* thread = _cpu()->thread;
    assert(thread);

    if((!sem->head && !sem->tail) || !((thread->sleep_next || thread->sleep_prev) || (sem->head == thread && sem->tail == thread)))
        return;

    if(sem->head == thread)
        sem->head = thread->sleep_next;
    else
        thread->sleep_prev->sleep_next = thread->sleep_next;

    if(sem->tail == thread)
        sem->tail = thread->sleep_prev;
    else
        thread->sleep_next->sleep_prev = thread->sleep_prev;

    thread->sleep_prev = thread->sleep_next = nullptr;
}

int semaphore_wait(semaphore_t* sem, bool interruptible) {
    if(!_cpu()->thread) {
        while(!semaphore_test(sem))
            pause();
        return 0;
    }

    bool int_state = interrupt_set(false);
    int ret = 0;
    spinlock_acquire(&sem->lock);

    if(--sem->i < 0) {
        insert_thread(sem, _cpu()->thread);
        sched_prepare_sleep(interruptible);
        spinlock_release(&sem->lock);

        if((ret = sched_yield())) {
            spinlock_acquire(&sem->lock);
            sem->i++;
            remove_current_thread(sem);
            spinlock_release(&sem->lock);
        }

        goto finish;
    }

    spinlock_release(&sem->lock);
finish:
    interrupt_set(int_state);
    return ret;
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

    if(++sem->i <= 0) {
        struct thread* thread;
        do
            if((thread = next_thread(sem))) {
                sched_wakeup(thread, 0);
                break;
            }
        while(thread);
    }

    spinlock_release(&sem->lock);
    interrupt_set(int_state);
}

