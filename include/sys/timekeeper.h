#ifndef _AMETHYST_SYS_TIMEKEEPER_H
#define _AMETHYST_SYS_TIMEKEEPER_H

#include <time.h>

void timekeeper_init(time_t (*tick)(), time_t ticks_per_us, time_t boot_time);
void timekeeper_switch(time_t (*tick)(), time_t ticks_per_us);

struct timespec timekeeper_time_from_boot(void);
struct timespec timekeeper_time(void);

#endif /* _AMETHYST_SYS_TIMEKEEPER_H */

