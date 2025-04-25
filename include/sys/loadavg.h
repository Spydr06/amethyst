#ifndef _AMETHYST_SYS_LOADAVG_H
#define _AMETHYST_SYS_LOADAVG_H

#include <stddef.h>

#include <sys/scheduler.h>

#define AVENRUN_COUNT 3

#define FSHIFT  11
#define FIXED_1 (1 << FSHIFT)

#define FIX_WHOLE(f) ((f) >> FSHIFT)
#define FIX_FRACT(f) ((((f) & (FIXED_1 - 1)) * 1000) / FIXED_1)

// 5 sec intervals
#define LOAD_FREQ (5 * QUANTUM_HZ)

#define EXP_1  1884 /* 1 / exp(5sec / 1min) as fixed-point */
#define EXP_5  2014 /* 1 / exp(5sec / 5min) */
#define EXP_15 2037 /* 1 / exp(5sec / 15min) */

extern size_t avenrun[AVENRUN_COUNT];

void get_avenrun(size_t loads[AVENRUN_COUNT], size_t offset, int shift);

static inline size_t calc_load(size_t load, size_t exp, size_t active) {
    size_t new_load = load * exp + active * (FIXED_1 - exp);
    if(active >= load)
        new_load += FIXED_1 - 1;

    return new_load / FIXED_1;
}

void calc_global_load(void);
void calc_global_load_tick(void);

#endif /* _AMETHYST_SYS_LOADAVG_H */

