#include <timer.h>

void sleep(uint64_t ms) {
    uint64_t current_time = millis();

    while(current_time + ms < millis()); // TODO: hook into scheduler
}

