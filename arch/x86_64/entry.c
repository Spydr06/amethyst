#include "arch/common/early_serial.h"
#include <arch/x86_64/early_serial.h>
#include <stdnoreturn.h>

noreturn void _start(void) {
    struct early_serial_dev serial;
    early_serial_init(&serial, EARLY_SERIAL_DEFAULT_PORT, EARLY_SERIAL_DEFAULT_BAUD_DIVISOR);

    while(1);
}

