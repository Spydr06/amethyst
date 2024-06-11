#include <drivers/char/keyboard.h>
#include <kernelio.h>
#include <ctype.h>

#ifdef __x86_64__
    #include <x86_64/cpu/idt.h>
#endif

#include <io/keyboard.h>

static const char convtab_capslock[] = {
    '\0', '\e', '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '=', '\b', '\t',
    'Q', 'W', 'E', 'R', 'T', 'Y', 'U', 'I', 'O', 'P', '[', ']', '\n', '\0', 'A', 'S',
    'D', 'F', 'G', 'H', 'J', 'K', 'L', ';', '\'', '`', '\0', '\\', 'Z', 'X', 'C', 'V',
    'B', 'N', 'M', ',', '.', '/', '\0', '\0', '\0', ' '
};

static const char convtab_shift[] = {
    '\0', '\e', '!', '@', '#', '$', '%', '^', '&', '*', '(', ')', '_', '+', '\b', '\t',
    'Q', 'W', 'E', 'R', 'T', 'Y', 'U', 'I', 'O', 'P', '{', '}', '\n', '\0', 'A', 'S',
    'D', 'F', 'G', 'H', 'J', 'K', 'L', ':', '"', '~', '\0', '|', 'Z', 'X', 'C', 'V',
    'B', 'N', 'M', '<', '>', '?', '\0', '\0', '\0', ' '
};

static const char convtab_shift_capslock[] = {
    '\0', '\e', '!', '@', '#', '$', '%', '^', '&', '*', '(', ')', '_', '+', '\b', '\t',
    'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '{', '}', '\n', '\0', 'a', 's',
    'd', 'f', 'g', 'h', 'j', 'k', 'l', ':', '"', '~', '\0', '|', 'z', 'x', 'c', 'v',
    'b', 'n', 'm', '<', '>', '?', '\0', '\0', '\0', ' '
};

static const char convtab_default[] = {
    '\0', '\e', '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '=', '\b', '\t',
    'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '[', ']', '\n', '\0', 'a', 's',
    'd', 'f', 'g', 'h', 'j', 'k', 'l', ';', '\'', '`', '\0', '\\', 'z', 'x', 'c', 'v',
    'b', 'n', 'm', ',', '.', '/', '\0', '\0', '\0', ' '
};

static void empty_handler(struct keyboard_event __unused) {}
static void (*keyboard_handler)(struct keyboard_event) = empty_handler;

static enum keyboard_flags flags = 0; 

cpu_status_t* keyboard_interrupt_handler(cpu_status_t* status) {
    bool interrup_state = interrupt_set(false);

    enum ps2_scan_code scan_code = get_ps2_scan_code();
    if(scan_code > 0x80) {
        scan_code -= 0x80;
        flags |= KB_FLAGS_RELEASED;
    }
    else 
        flags &= ~KB_FLAGS_RELEASED;

    keycode_t keycode = scan_code;
    char ascii = 0;
    switch((uint8_t) scan_code) {
        case KEY_LEFT_SHIFT:
            if(flags & KB_FLAGS_RELEASED)
                flags &= ~KB_FLAGS_LSHIFT;
            else
                flags |= KB_FLAGS_LSHIFT;
            break;
        case KEY_RIGHT_SHIFT:
            if(flags & KB_FLAGS_RELEASED)
                flags &= ~KB_FLAGS_RSHIFT;
            else
                flags |= KB_FLAGS_RSHIFT;
            break;
        case KEY_CAPS_LOCK:
            if(!(flags & KB_FLAGS_RELEASED))
                flags ^= KB_FLAGS_CAPSLOCK;
            break;
        case KEY_LEFT_CTRL:
            if(flags & KB_FLAGS_RELEASED)
                flags &= ~KB_FLAGS_LCTRL;
            else
                flags |= KB_FLAGS_LCTRL;
            break;
        case KEY_LEFT_ALT:
            if(flags & KB_FLAGS_RELEASED)
                flags &= ~KB_FLAGS_LALT;
            else
                flags |= KB_FLAGS_LALT;
            break;
        case 0xe0:
            goto finish; // TODO: handle extended keyset
        default:
            if(flags & KB_FLAGS_CAPSLOCK && flags & KB_FLAGS_SHIFT)
                ascii = convtab_shift_capslock[scan_code];
            else if(flags & KB_FLAGS_SHIFT)
                ascii = convtab_shift[scan_code];
            else if(flags & KB_FLAGS_CAPSLOCK)
                ascii = convtab_capslock[scan_code];
            else 
                ascii = convtab_default[scan_code];
    }

    keyboard_handler((struct keyboard_event){
        .ascii = ascii,
        .keycode = keycode,
        .flags = flags
    });
    
finish:
    interrupt_set(interrup_state);
    return status;
}

void keyboard_set_event_handler(void (*handler)(struct keyboard_event)) {
    keyboard_handler = handler;
}
