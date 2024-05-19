#ifndef _AMETHYST_SYS_EARLY_TIMER_H
#define _AMETHYST_SYS_EARLY_TIMER_H

#include <time.h>

void early_timer_init(void);
void early_timer_deinit(void);

time_t early_timer_millis(void);

#endif /* _AMETHYST_SYS_EARLY_TIMER_H */

