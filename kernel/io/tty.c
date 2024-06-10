#include <io/tty.h>

#include <sys/tty.h>
#include <drivers/video/console.h>

#ifdef __x86_64__
    #include <x86_64/init/early_serial.h>
#endif

#include <assert.h>
#include <stddef.h>

#define NUM_VGA_TTYS 1
#define NUM_SERIAL_TTYS 1

static const char* const vga_tty_names[NUM_VGA_TTYS] = { "tty0" };
static struct tty* vga_ttys[NUM_VGA_TTYS];
static unsigned selected_vga_tty = 0;

#ifdef __x86_64__
static const char* const serial_tty_names[NUM_SERIAL_TTYS] = { "ttyS0" };
static struct tty* serial_ttys[NUM_SERIAL_TTYS];
static unsigned selected_serial_tty = 0;
#endif

static void tty_putchar(int c) {
    tty_process(vga_ttys[0], (char) c);
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

    kernelio_writer = tty_putchar;
}
