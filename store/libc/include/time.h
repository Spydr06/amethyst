#ifndef _TIME_H
#define _TIME_H

#ifdef __cplusplus
extern "C" {
#endif

#include <bits/alltypes.h>

time_t time(time_t *tloc);

int nanosleep(const struct timespec *rqtp, struct timespec *rmtp);

#ifdef __cplusplus
}
#endif

#endif /* _TIME_H */

