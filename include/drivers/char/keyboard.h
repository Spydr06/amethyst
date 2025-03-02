#ifndef _AMETHYST_DRIVERS_CHAR_KEYBOARD_H
#define _AMETHYST_DRIVERS_CHAR_KEYBOARD_H

#include <stdint.h>
#include <ringbuffer.h>

#include <sys/semaphore.h>
#include <sys/spinlock.h>

#define MAX_KEYBOARDS 10

enum keycode : uint8_t {
    KEYCODE_RESERVED = 0,
	KEYCODE_ESCAPE = 1,
	KEYCODE_1 = 2,
	KEYCODE_2 = 3,
	KEYCODE_3 = 4,
	KEYCODE_4 = 5,
	KEYCODE_5 = 6,
	KEYCODE_6 = 7,
	KEYCODE_7 = 8,
	KEYCODE_8 = 9,
	KEYCODE_9 = 10,
	KEYCODE_0 = 11,
	KEYCODE_MINUS = 12,
	KEYCODE_EQUAL = 13,
	KEYCODE_BACKSPACE = 14,
	KEYCODE_TAB = 15,
	KEYCODE_Q = 16,
	KEYCODE_W = 17,
	KEYCODE_E = 18,
	KEYCODE_R = 19,
	KEYCODE_T = 20,
	KEYCODE_Y = 21,
	KEYCODE_U = 22,
	KEYCODE_I = 23,
	KEYCODE_O = 24,
	KEYCODE_P = 25,
	KEYCODE_LBRACE = 26,
	KEYCODE_RBRACE = 27,
	KEYCODE_ENTER = 28 ,
	KEYCODE_LCTRL = 29,
	KEYCODE_A = 30,
	KEYCODE_S = 31,
	KEYCODE_D = 32,
	KEYCODE_F = 33,
	KEYCODE_G = 34,
	KEYCODE_H = 35,
	KEYCODE_J = 36,
	KEYCODE_K = 37,
	KEYCODE_L = 38,
	KEYCODE_SEMICOLON = 39,
	KEYCODE_APOSTROPHE = 40,
	KEYCODE_GRAVE = 41,
	KEYCODE_LSHIFT = 42,
	KEYCODE_BACKSLASH = 43,
	KEYCODE_Z = 44,
	KEYCODE_X = 45,
	KEYCODE_C = 46,
	KEYCODE_V = 47,
	KEYCODE_B = 48,
	KEYCODE_N = 49,
	KEYCODE_M = 50,
	KEYCODE_COMMA = 51,
	KEYCODE_DOT = 52,
	KEYCODE_SLASH = 53,
	KEYCODE_RSHIFT = 54,
	KEYCODE_KEYPADASTERISK = 55,
	KEYCODE_LALT = 56,
	KEYCODE_SPACE = 57,
	KEYCODE_CAPSLOCK = 58,
	KEYCODE_F1 = 59,
	KEYCODE_F2 = 60,
	KEYCODE_F3 = 61,
	KEYCODE_F4 = 62,
	KEYCODE_F5 = 63,
	KEYCODE_F6 = 64,
	KEYCODE_F7 = 65,
	KEYCODE_F8 = 66,
	KEYCODE_F9 = 67,
	KEYCODE_F10 = 68,
	KEYCODE_NUMLOCK = 69,
	KEYCODE_SCROLLLOCK = 70,
	KEYCODE_KEYPAD7 = 71,
	KEYCODE_KEYPAD8 = 72,
	KEYCODE_KEYPAD9 = 73,
	KEYCODE_KEYPADMINUS = 74,
	KEYCODE_KEYPAD4 = 75,
	KEYCODE_KEYPAD5 = 76,
	KEYCODE_KEYPAD6 = 77,
	KEYCODE_KEYPADPLUS = 78,
	KEYCODE_KEYPAD1 = 79,
	KEYCODE_KEYPAD2 = 80,
	KEYCODE_KEYPAD3 = 81,
	KEYCODE_KEYPAD0 = 82,
	KEYCODE_KEYPADDOT = 83,
	KEYCODE_F11 = 87,
	KEYCODE_F12 = 88,
	KEYCODE_KEYPADENTER = 96,
	KEYCODE_RCTRL = 97,
	KEYCODE_KEYPADSLASH = 98,
	KEYCODE_SYSREQ = 99,
	KEYCODE_RALT = 100,
	KEYCODE_LINEFEED  = 101,
	KEYCODE_HOME = 102,
	KEYCODE_UP = 103,
	KEYCODE_PAGEUP = 104,
	KEYCODE_LEFT = 105,
	KEYCODE_RIGHT = 106,
	KEYCODE_END = 107,
	KEYCODE_DOWN = 108,
	KEYCODE_PAGEDOWN = 109,
	KEYCODE_INSERT = 110,
	KEYCODE_DELETE = 111,
};

enum keyboard_flags : uint8_t {
    KEYBOARD_EVENT_RELEASED = 0x01,
    KEYBOARD_EVENT_LALT     = 0x02,
    KEYBOARD_EVENT_RALT     = 0x04,
    KEYBOARD_EVENT_LSHIFT   = 0x08,
    KEYBOARD_EVENT_RSHIFT   = 0x10,
    KEYBOARD_EVENT_LCTRL    = 0x20,
    KEYBOARD_EVENT_RCTRL    = 0x40,
    KEYBOARD_EVENT_CAPSLOCK = 0x80
};

struct keyboard_event {
    enum keyboard_flags flags;
    uint8_t keycode; 
};

struct keyboard {
    ringbuffer_t event_buffer;
    semaphore_t semaphore;
    spinlock_t lock;
    enum keyboard_flags flags;
};

extern struct keyboard keyboard_console;

void keyboard_driver_init(void);
int keyboard_register(struct keyboard* kb);

int keyboard_init(struct keyboard* kb);
void keyboard_deinit(struct keyboard* kb);

int keyboard_wait(struct keyboard* kb, struct keyboard_event* event);
void keyboard_event(struct keyboard* kb, struct keyboard_event event);

char keyboard_event_as_ascii(struct keyboard_event event);

#endif /* _AMETHYST_DRIVERS_CHAR_KEYBOARD_H */

