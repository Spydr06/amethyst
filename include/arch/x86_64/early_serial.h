#ifndef _ARCH_X86_64_EARLY_SERIAL_H
#define _ARCH_X86_64_EARLY_SERIAL_H

#include <stdint.h>

#include <arch/common/early_serial.h>

#define EARLY_SERIAL_DEFAULT_PORT 0x03f8
#define EARLY_SERIAL_DEFAULT_BAUD_DIVISOR 9600

struct early_serial_dev {
    uint32_t port;
    uint32_t baud;
};

#endif /* _ARCH_X86_64_EARLY_SERIAL_H */

