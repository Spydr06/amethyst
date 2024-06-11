#ifndef _AMETHYST_DRIVERS_CHAR_KEYBOARD_H
#define _AMETHYST_DRIVERS_CHAR_KEYBOARD_H

#include <stdint.h>
#include <x86_64/cpu/cpu.h>

// PS2 scan codes
enum ps2_scan_code : uint8_t {
    KEY_NULL = 0,
    KEY_ESCAPE,
    KEY_1, KEY_2, KEY_3, KEY_4, KEY_5, KEY_6, KEY_7, KEY_8, KEY_9, KEY_0,
    KEY_MINUS, KEY_EQUALS,
    KEY_BACKSPACE,
    KEY_TAB,
    KEY_Q, KEY_W, KEY_E, KEY_R, KEY_T, KEY_Y, KEY_U, KEY_I, KEY_O, KEY_P,
    KEY_LEFT_BRACKET, KEY_RIGHT_BRACKET,
    KEY_ENTER,
    KEY_LEFT_CTRL,
    KEY_A, KEY_S, KEY_D, KEY_F, KEY_G, KEY_H, KEY_J, KEY_K, KEY_L,
    KEY_SEMICOLON, KEY_APOSTROPHE, KEY_BACKTICK,
    KEY_LEFT_SHIFT,
    KEY_BACKSLASH,
    KEY_Z, KEY_X, KEY_C, KEY_V, KEY_B, KEY_N, KEY_M,
    KEY_COMMA, KEY_PERIOD, KEY_SLASH,
    KEY_RIGHT_SHIFT,
    KEY_ASTERISK,
    KEY_LEFT_ALT,
    KEY_SPACE,
    KEY_CAPS_LOCK,
    KEY_F1, KEY_F2, KEY_F3, KEY_F4, KEY_F5, KEY_F6, KEY_F7, KEY_F8, KEY_F9, KEY_F10,
    KEY_NUM_LOCK, KEY_SCROLL_LOCK,
    KEY_HOME,
    KEY_UP, KEY_PAGE_UP,
    KEY_UNKNOWN0,
    KEY_LEFT,
    KEY_UNKNOWN1,
    KEY_RIGHT,
    KEY_PLUS,
    KEY_END,
    KEY_DOWN, KEY_PAGE_DOWN,
    KEY_INSERT,
    KEY_DELETE,
    KEY_UNKNOWN2, KEY_UNKNOWN3, KEY_UNKNOWN4,
    KEY_F11, KEY_F12,

    KEY_UNDEFINED,
};

struct keyboard_event;

cpu_status_t* keyboard_interrupt_handler(cpu_status_t* status);

void keyboard_set_event_handler(void (*handler)(struct keyboard_event));

// platform-specific:
enum ps2_scan_code get_ps2_scan_code(void);

#endif /* _AMETHYST_DRIVERS_CHAR_KEYBOARD_H */

