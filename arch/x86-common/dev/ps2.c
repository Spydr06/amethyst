#include <drivers/char/keyboard.h>
#include "arch/x86-common/io.h"

enum ps2_scan_code get_ps2_scan_code(void) {
    return inb(0x60);
}

