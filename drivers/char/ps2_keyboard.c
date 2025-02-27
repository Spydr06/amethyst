#include <drivers/char/ps2.h>
#include <drivers/char/keyboard.h>

#include <x86_64/cpu/idt.h>
#include <x86_64/dev/apic.h>
#include <x86_64/dev/io.h>

#include <assert.h>
#include <kernelio.h>

static const uint8_t SCANCODE_EXT = 0xe0;

static uint8_t keycodes[128] = {
	KEYCODE_RESERVED,
	KEYCODE_ESCAPE,
	KEYCODE_1,
	KEYCODE_2,
	KEYCODE_3,
	KEYCODE_4,
	KEYCODE_5,
	KEYCODE_6,
	KEYCODE_7,
	KEYCODE_8,
	KEYCODE_9,
	KEYCODE_0,
	KEYCODE_MINUS,
	KEYCODE_EQUAL,
	KEYCODE_BACKSPACE,
	KEYCODE_TAB,
	KEYCODE_Q,
	KEYCODE_W,
	KEYCODE_E,
	KEYCODE_R,
	KEYCODE_T,
	KEYCODE_Y,
	KEYCODE_U,
	KEYCODE_I,
	KEYCODE_O,
	KEYCODE_P,
	KEYCODE_LBRACE,
	KEYCODE_RBRACE,
	KEYCODE_ENTER,
	KEYCODE_LCTRL,
	KEYCODE_A,
	KEYCODE_S,
	KEYCODE_D,
	KEYCODE_F,
	KEYCODE_G,
	KEYCODE_H,
	KEYCODE_J,
	KEYCODE_K,
	KEYCODE_L,
	KEYCODE_SEMICOLON,
	KEYCODE_APOSTROPHE,
	KEYCODE_GRAVE,
	KEYCODE_LSHIFT,
	KEYCODE_BACKSLASH,
	KEYCODE_Z,
	KEYCODE_X,
	KEYCODE_C,
	KEYCODE_V,
	KEYCODE_B,
	KEYCODE_N,
	KEYCODE_M,
	KEYCODE_COMMA,
	KEYCODE_DOT,
	KEYCODE_SLASH,
	KEYCODE_RSHIFT,
	KEYCODE_KEYPADASTERISK,
	KEYCODE_LALT,
	KEYCODE_SPACE,
	KEYCODE_CAPSLOCK,
	KEYCODE_F1,
	KEYCODE_F2,
	KEYCODE_F3,
	KEYCODE_F4,
	KEYCODE_F5,
	KEYCODE_F6,
	KEYCODE_F7,
	KEYCODE_F8,
	KEYCODE_F9,
	KEYCODE_F10,
	KEYCODE_NUMLOCK,
	KEYCODE_SCROLLLOCK,
	KEYCODE_KEYPAD7,
	KEYCODE_KEYPAD8,
	KEYCODE_KEYPAD9,
	KEYCODE_KEYPADMINUS,
	KEYCODE_KEYPAD4,
	KEYCODE_KEYPAD5,
	KEYCODE_KEYPAD6,
	KEYCODE_KEYPADPLUS,
	KEYCODE_KEYPAD1,
	KEYCODE_KEYPAD2,
	KEYCODE_KEYPAD3,
	KEYCODE_KEYPAD0,
	KEYCODE_KEYPADDOT,
	0, 0, 0,
	KEYCODE_F11,
	KEYCODE_F12
};

static uint8_t ext_keycodes[128] = {
    [0x1C] = KEYCODE_KEYPADENTER,
	[0x1D] = KEYCODE_RCTRL, 
	[0x35] = KEYCODE_KEYPADSLASH,
	[0x38] = KEYCODE_RALT,
	[0x47] = KEYCODE_HOME,
	[0x48] = KEYCODE_UP,
	[0x49] = KEYCODE_PAGEUP,
	[0x4B] = KEYCODE_LEFT,
	[0x4D] = KEYCODE_RIGHT,
	[0x4F] = KEYCODE_END,
	[0x50] = KEYCODE_DOWN,
	[0x51] = KEYCODE_PAGEDOWN,
	[0x52] = KEYCODE_INSERT,
	[0x53] = KEYCODE_DELETE
};

static bool extended = false;
static struct keyboard keyboard;

static void keyboard_isr(struct cpu_context* __unused) {
    uint8_t scancode = inb(PS2_PORT_DATA);
    if(scancode == SCANCODE_EXT) {
        extended = true;
        return;
    }

    struct keyboard_event event = {0};

    bool released = !!(scancode & 0x80);
    if(released) {
        scancode &= 0x7f;
        event.flags |= KEYBOARD_EVENT_RELEASED;
    }
    
    if(extended) {
        extended = false;
        event.keycode = ext_keycodes[scancode];
    }
    else
        event.keycode = keycodes[scancode];

    if(!event.keycode)
        return;

    keyboard_event(&keyboard, event);
    klog(DEBUG, "keycode: %hhx", event.keycode);
}

void ps2_keyboard_init(void) {
    struct isr* isr = interrupt_allocate(keyboard_isr, apic_send_eoi, IPL_KEYBOARD);
    assert(isr);
    io_apic_register_interrupt(KEYBOARD_INTERRUPT, isr->id & 0xff, _cpu()->id, false);
    keyboard_init(&keyboard);
    keyboard_register(&keyboard);
    klog(INFO, "PS/2 keyboard interrupt enabled with vector %lu", isr->id & 0xff);
}

