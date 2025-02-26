#ifndef _AMETHYST_LIBC_STDLIB_H
#define _AMETHYST_LIBC_STDLIB_H

#include <rand.h>

int atoi(const char *s);

static inline int abs(int x) {
    return x < 0 ? -x : x;
}

#endif /* _AMETHYST_LIBC_STDLIB_H */

