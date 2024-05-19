#include <sys/timekeeper.h>
#include <sys/early_timer.h>

static time_t boot_time;
static time_t ticks_per_us;
static time_t init_clock_ticks;
static time_t elapsed_early_ms;
static time_t (*clock_ticks)(void);

void timekeeper_init(time_t (*tick)(), time_t _ticks_per_us, time_t _boot_time) {
    boot_time = _boot_time;
    init_clock_ticks = tick();
    ticks_per_us = _ticks_per_us;
    elapsed_early_ms = early_timer_millis();
    clock_ticks = tick;
}

struct timespec timekeeper_time_from_boot(void) {
    if(clock_ticks) {
        time_t ticks = clock_ticks() - init_clock_ticks;
        time_t us_passed = (ticks / ticks_per_us) + elapsed_early_ms * 1000;
        return (struct timespec) {
            .s = us_passed / 1'000'000,
            .ns = (us_passed * 1000) % 1'000'000'000
        };
    }
    else
        return (struct timespec) {
            .s = early_timer_millis() / 1000,
            .ns = (early_timer_millis() * 1'000'000) % 1'000'000'000
        };
}

struct timespec timekeeper_time(void) {
     struct timespec from_boot = timekeeper_time_from_boot();
     struct timespec unix;
     unix.s = boot_time;
     unix.ns = 0;
     return timespec_add(unix, from_boot);
}

