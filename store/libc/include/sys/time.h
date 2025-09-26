#ifndef _SYS_TIME_H
#define _SYS_TIME_H

#include <amethyst/timeval.h>

int gettimeofday(struct timeval *tv, void *restrict tzp);

#endif /* _SYS_TIME_H */

