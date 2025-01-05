#include <io/tty.h>

#include <sys/tty.h>
#include <io/keyboard.h>
#include <drivers/video/console.h>

#ifdef __x86_64__
    #include <x86_64/init/early_serial.h>
#endif

#include <assert.h>
#include <stddef.h>
#include <string.h>
#include <ctype.h>

#define NUM_VGA_TTYS 1
#define NUM_SERIAL_TTYS 1

static const char* const vga_tty_names[NUM_VGA_TTYS] = { "tty0" };
static struct tty* vga_ttys[NUM_VGA_TTYS];

#ifdef __x86_64__
static const char* const serial_tty_names[NUM_SERIAL_TTYS] = { "ttyS0" };
static struct tty* serial_ttys[NUM_SERIAL_TTYS];
#endif

static struct tty* selected_tty;

static void tty_putchar(int c) {
    tty_process(vga_ttys[0], (char) c);
}

static void tty_keyboard_handler(struct keyboard_event event) {
    if(event.flags & KB_FLAGS_RELEASED)
        return;

    if(event.ascii) {
        if(event.flags & KB_FLAGS_CTRL) {
            if(!(isalpha(event.ascii) || event.ascii == '[' || event.ascii == '\\'))
                return;

            event.ascii = event.ascii >= 'a' ? event.ascii - 0x60 : event.ascii - 0x40;
        }

        tty_process(selected_tty, event.ascii);
        return;
    }

    const char* tmp;
    switch(event.keycode) {
        case KEY_HOME:
            tmp = "\e[1~";
            break;
        case KEY_INSERT:
            tmp = "\e[2~";
            break;
        case KEY_DELETE:
            tmp = "\e[3~";
            break;
        case KEY_END:
            tmp = "\e[4~";
            break;
        case KEY_PAGE_UP:
            tmp = "\e[5~";
            break;
        case KEY_PAGE_DOWN:
            tmp = "\e[6~";
            break;
        case KEY_UP:
            tmp = "\e[A";
            break;
        case KEY_DOWN:
            tmp = "\e[B";
            break;
        case KEY_RIGHT:
            tmp = "\e[C";
            break;
        case KEY_LEFT:
            tmp = "\e[D";
            break;
        default:
            return;
    }

    size_t len = strlen(tmp);
    for(size_t i = 0; i < len; i++)
        tty_process(selected_tty, tmp[i]);
} 

void create_ttys(void) {
    vga_console_disable_writer_propagation();
    for(size_t i = 0; i < NUM_VGA_TTYS; i++) {
        vga_ttys[i] = tty_create(vga_tty_names[i], vga_console_ttywrite, nullptr, nullptr);
        assert(vga_ttys[i]);

        vga_console_winsize(&vga_ttys[i]->winsize);
    }

    keyboard_set_event_handler(tty_keyboard_handler);

#ifdef __x86_64__
    for(size_t i = 0; i < NUM_SERIAL_TTYS; i++) {
        serial_ttys[i] = tty_create(serial_tty_names[i], early_serial_ttywrite, nullptr, nullptr);
        assert(serial_ttys[i]);
    }

#endif

    selected_tty = vga_ttys[0];
    kernelio_writer = tty_putchar;
}
