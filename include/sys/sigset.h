#ifndef _AMETHYST_SYS_SIGSET_H
#define _AMETHYST_SYS_SIGSET_H

#include <amethyst/signal.h>
#include <assert.h>
#include <stddef.h>

static inline bool sigset_get(const volatile sigset_t *set, signo_t signo) {
    assert(signo > 0 && signo <= _AMETHYST_NSIG);
    return (set->set[(signo - 1) / 64] & 1ul << ((signo - 1) & 63)) != 0;
}

static inline void sigset_set(volatile sigset_t *set, signo_t signo) {
    assert(signo > 0 && signo <= _AMETHYST_NSIG);
    set->set[(signo - 1) / 64] |= 1ul << ((signo - 1) & 63);
}

static inline void sigset_unset(volatile sigset_t *set, signo_t signo) {
    assert(signo > 0 && signo <= _AMETHYST_NSIG);
    set->set[(signo - 1) / 64] &= ~(1ul << ((signo - 1) & 63));
}

static inline void sigset_block(volatile sigset_t *set, const sigset_t* new) {
    for(size_t i = 0; i < __len(set->set); i++)
        set->set[i] |= new->set[i];
}

static inline void sigset_unblock(volatile sigset_t *set, const sigset_t *new) {
    for(size_t i = 0; i < __len(set->set); i++)
        set->set[i] &= ~(new->set[i]);
}

#endif /* _AMETHYST_SYS_SIGSET_H */
