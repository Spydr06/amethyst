#ifndef _ARCH_COMMON_EARLY_SERIAL_H
#define _ARCH_COMMON_EARLY_SERIAL_H

#include <stdint.h>
#include <stddef.h>
#include <cdefs.h>

struct early_serial_dev;

int early_serial_init(struct early_serial_dev *dev, uint32_t port, uint32_t divisor);

int early_serial_putchar(struct early_serial_dev *dev, int ch);
ssize_t early_serial_write(struct early_serial_dev *dev, const uint8_t *data, size_t size);
ssize_t early_serial_puts(struct early_serial_dev *dev, const char *str);

#endif /* _ARCH_COMMON_EARLY_SERIAL_H */

