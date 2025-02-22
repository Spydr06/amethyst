#ifndef _TIME_H
#define _TIME_H

#ifdef __cplusplus
extern "C" {
#endif

#include <bits/alltypes.h>

#define CLOCKS_PER_SEC ((clock_t) 1'000'000L)

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
    long __tm_gmtoff;
    const char *__tm_zone;
};

clock_t clock(void);

double difftime(time_t lhs, time_t rhs);

time_t mktime(struct tm *tm);
time_t timegm(struct tm *tm);

time_t time(time_t *result);

int timespec_get(struct timespec *ts, int base);
int timespec_getres(struct timespec *ts, int base);

size_t strftime(char *restrict str, size_t max, const char *restrict fmt, const struct tm *restrict tm);

struct tm *gmtime(const time_t *time);
struct tm *gmtime_r(const time_t *time, struct tm *buf);
struct tm *gmtime_s(const time_t *restrict time, struct tm *restrict result);

struct tm *localtime(const time_t *time);
struct tm *localtime_r(const time_t *time, struct tm *buf);
struct tm *localtime_s(const time_t *restrict time, struct tm *restrict result);

char *asctime(const struct tm *tm);
char *asctime_s(char *str, size_t size, const struct tm *tm);

char *ctime(const time_t *time);
char *ctime_s(char *str, size_t size, const time_t *time);

#ifdef __cplusplus
}
#endif

#endif /* _TIME_H */
