#ifndef _AMETHYST_IO_KEYBOARD_H
#define _AMETHYST_IO_KEYBOARD_H

#include <stdint.h>
#include <drivers/char/keyboard.h>

typedef enum ps2_scan_code keycode_t;

enum keyboard_flags : uint8_t {
    KB_FLAGS_RELEASED = 1 << 0,
    KB_FLAGS_LCTRL    = 1 << 1,
    KB_FLAGS_RCTRL    = 1 << 2,
    KB_FLAGS_CTRL     = (KB_FLAGS_LCTRL | KB_FLAGS_RCTRL),

    KB_FLAGS_LALT     = 1 << 3,
    KB_FLAGS_RALT     = 1 << 4,
    KB_FLAGS_ALT      = (KB_FLAGS_LALT | KB_FLAGS_RALT),
    
    KB_FLAGS_LSHIFT   = 1 << 5,
    KB_FLAGS_RSHIFT   = 1 << 6,
    KB_FLAGS_SHIFT    = (KB_FLAGS_LSHIFT | KB_FLAGS_RSHIFT),

    KB_FLAGS_CAPSLOCK = 1 << 7
};

struct keyboard_event {
    char ascii;
    keycode_t keycode;
    enum keyboard_flags flags;
};

#endif /* _AMETHYST_IO_KEYBOARD_H */

