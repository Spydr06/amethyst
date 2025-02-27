#ifndef _AMETHYST_DRIVERS_CHAR_MOUSE
#define _AMETHYST_DRIVERS_CHAR_MOUSE

#include <stdbool.h>
#include <stdint.h>

struct mouse_event {
    int16_t x;
    int16_t y;
    int16_t z;

    bool left    : 1;
    bool right   : 1;
    bool middle  : 1;
    bool button4 : 1;
    bool button5 : 1;
    uint8_t __reserved : 3;
};

struct mouse {

};

void mouse_init(struct mouse* mouse);

void mouse_event(struct mouse* mouse, struct mouse_event event);

#endif /* _AMETHYST_DRIVERS_CHAR_MOUSE */

