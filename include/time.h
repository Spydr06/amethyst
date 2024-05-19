#ifndef _AMETHYST_LIBK_TIME_H
#define _AMETHYST_LIBK_TIME_H

#include <stdint.h>

typedef int64_t time_t;

struct timespec {
    time_t s;
    time_t ns;
};

struct tm {
    int tm_sec;
    int tm_min;
    int tm_hour;
    int tm_mday;
    int tm_mon;
    int tm_year;
    int tm_wday;
    int tm_yday;
    int tm_isdst;
};

time_t mktime(struct tm* tm);

static inline struct timespec timespec_add(struct timespec a, struct timespec b) {
    return (struct timespec) {
        .ns = (a.ns + b.ns) % 1'000'000'000,
        .s = a.s + b.s + (a.ns + b.ns) / 1'000'000'000
    };
}

#endif /* _AMETHYST_LIBK_TIME_H */

