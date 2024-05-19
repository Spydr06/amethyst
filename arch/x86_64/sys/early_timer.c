#include <sys/early_timer.h>

#include <x86_64/dev/pic.h>

time_t _millis = 0;

void early_timer_init(void) {
    pic_init();
}

time_t early_timer_millis(void) {
    return _millis;
}

void early_timer_deinit(void) {
    pic_disable(); 
}

