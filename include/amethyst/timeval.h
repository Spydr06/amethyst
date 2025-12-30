#ifndef _AMETHYST_TIMEVAL_H
#define _AMETHYST_TIMEVAL_H

#include <time.h>
#include <stdint.h>

typedef int64_t suseconds_t;

struct timeval {
    time_t tv_sec;
    suseconds_t tv_usec;
};

#endif /* _AMETHYST_TIMEVAL_H */

