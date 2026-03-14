#include "amethyst/signal.h"
#include "sys/sigset.h"
#include <sys/signal.h>
#include <sys/spinlock.h>

#include <mem/slab.h>

#include <assert.h>
#include <errno.h>
#include <memory.h>

int sig_queue_init(struct sig_queue* queue) {
    memset(queue, 0, sizeof(struct sig_queue));
    spinlock_init(queue->lock);
    return 0;
}

void sig_queue_delete(struct sig_queue *queue) {
    sigset_t mask;
    memset(&mask, 0, sizeof(sigset_t));

    // dequeue remaining signals
    while(!signal_acquire(queue, nullptr, &mask));
}

int signal_raise(struct sig_queue *queue, const struct siginfo *sig) {
    if(!sig || sig->si_signo <= 0 || sig->si_signo >= _AMETHYST_NSIG)
        return EINVAL;

    spinlock_acquire(&queue->lock);
    if(sigset_get(&queue->pending, sig->si_signo))
        return EEXIST; // signal already raised

    vmemcpy(&queue->signals[sig->si_signo - 1], sig, sizeof(struct siginfo));
    sigset_set(&queue->pending, sig->si_signo);

    spinlock_release(&queue->lock);
    return 0;
}

int signal_acquire(struct sig_queue *queue, struct siginfo *sig, volatile const sigset_t *mask) {
    spinlock_acquire(&queue->lock);
    
    for(int i = 1; i <= _AMETHYST_NSIG; i++) {
        if(!sigset_get(&queue->pending, i))
            continue;
        if(mask && sigset_get(mask, i))
            continue; // signal blocked

        vmemcpy(sig, &queue->signals[i - 1], sizeof(struct siginfo));
        sigset_unset(&queue->pending, i);

        spinlock_release(&queue->lock);
        return 0;
    }

    spinlock_release(&queue->lock);
    return ENOENT;
}

bool signal_pending(struct sig_queue *queue) {
    spinlock_acquire(&queue->lock);

    sigset_t empty_set = {0};
    bool empty = vmemcmp(&queue->pending, &empty_set, sizeof(sigset_t)) == 0;

    spinlock_release(&queue->lock);
    return !empty;
}
