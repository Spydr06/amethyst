#ifndef _AMETHYST_LIBK_TIME_H
#define _AMETHYST_LIBK_TIME_H

#include <stdint.h>

typedef int64_t time_t;

struct timespec {
    time_t s;
    time_t ns;
};

#endif /* _AMETHYST_LIBK_TIME_H */

