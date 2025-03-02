#include "kernelio.h"
#include <io/tty.h>

#include <drivers/char/keyboard.h>
#include <drivers/video/console.h>
#include <sys/mutex.h>
#include <sys/scheduler.h>
#include <sys/tty.h>

#ifdef __x86_64__
    #include <x86_64/init/early_serial.h>
#endif

#include <assert.h>
#include <stddef.h>
#include <string.h>

#define NUM_VGA_TTYS 1
#define NUM_SERIAL_TTYS 1

static const char* const vga_tty_names[NUM_VGA_TTYS] = { "tty0" };
static struct tty* vga_ttys[NUM_VGA_TTYS];

#ifdef __x86_64__
static const char* const serial_tty_names[NUM_SERIAL_TTYS] = { "ttyS0" };
static struct tty* serial_ttys[NUM_SERIAL_TTYS];
#endif

static struct tty* selected_tty;

static struct thread* tty_thread;

static void tty_putchar(int c) {
    struct tty* tty = vga_ttys[0];
    
    char ch = c;
    tty->write_to_device(tty, &ch, 1);
}

static __noreturn void tty_thread_callback(void) {
    for(;;) {
        struct keyboard_event event;
        keyboard_wait(&keyboard_console, &event);
        if(event.flags & KEYBOARD_EVENT_RELEASED)
            continue;
        
        char ascii = keyboard_event_as_ascii(event);
        if(ascii) {
            tty_process(selected_tty, ascii);
            continue;
        }

        const char* tmp;
        switch(event.keycode) {
            case KEYCODE_HOME:
                tmp = "\e[1~";
                break;
            case KEYCODE_INSERT:
                tmp = "\e[2~";
                break;
            case KEYCODE_DELETE:
                tmp = "\e[3~";
                break;
            case KEYCODE_END:
                tmp = "\e[4~";
                break;
            case KEYCODE_PAGEUP:
                tmp = "\e[5~";
                break;
            case KEYCODE_PAGEDOWN:
                tmp = "\e[6~";
                break;
            case KEYCODE_UP:
                tmp = "\e[A";
                break;
            case KEYCODE_DOWN:
                tmp = "\e[B";
                break;
            case KEYCODE_RIGHT:
                tmp = "\e[C";
                break;
            case KEYCODE_LEFT:
                tmp = "\e[D";
                break;
            default:
                continue;
        }

        size_t len = strlen(tmp);
        for(size_t i = 0; i < len; i++)
            tty_process(selected_tty, tmp[i]);
    }
}

void create_ttys(void) {
    vga_console_disable_writer_propagation();
    for(size_t i = 0; i < NUM_VGA_TTYS; i++) {
        vga_ttys[i] = tty_create(vga_tty_names[i], vga_console_ttywrite, nullptr, nullptr);
        assert(vga_ttys[i]);

        vga_console_winsize(&vga_ttys[i]->winsize);
    }

#ifdef __x86_64__
    for(size_t i = 0; i < NUM_SERIAL_TTYS; i++) {
        serial_ttys[i] = tty_create(serial_tty_names[i], early_serial_ttywrite, nullptr, nullptr);
        assert(serial_ttys[i]);
    }
#endif

    selected_tty = vga_ttys[0];
    kernelio_writer = tty_putchar;

    tty_thread = thread_create(tty_thread_callback, PAGE_SIZE * 16, 0, nullptr, nullptr);
    assert(tty_thread);
    sched_queue(tty_thread);
}

