#ifndef _AMETHYST_SIGNAL_H
#define _AMETHYST_SIGNAL_H

#include <stddef.h>
#include <stdint.h>

struct stack {
    void* base;
    int flags;
    size_t size;
};

struct sigset {
    uint64_t sig[16];
};

#endif /* _AMETHYST_SIGNAL_H */

