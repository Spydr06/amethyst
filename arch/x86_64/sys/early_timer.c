#include <sys/early_timer.h>

#include <x86_64/dev/pit.h>

time_t _millis = 0;

void early_timer_init(void) {
    pit_init(SYSTEM_TICK_FREQUENCY);
}

time_t early_timer_millis(void) {
    return _millis;
}

void early_timer_deinit(void) {
    // nothing to do here
}

