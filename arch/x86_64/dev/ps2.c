#include <drivers/char/keyboard.h>
#include <x86_64/dev/io.h>

enum ps2_scan_code get_ps2_scan_code(void) {
    return inb(0x60);
}

