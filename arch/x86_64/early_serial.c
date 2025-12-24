#include <arch/x86_64/early_serial.h>
#include <arch/x86_64/io.h>
#include <arch/x86_64/cpu/intrin.h>

#include <errno.h>
#include <memory.h>

#define DEFAULT_BAUD 115200

#define TXR 0 /*  Transmit register (WRITE) */
#define RXR 0 /*  Receive register  (READ)  */
#define IER 1 /*  Interrupt Enable          */
#define IIR 2 /*  Interrupt ID              */
#define FCR 2 /*  FIFO control              */
#define LCR 3 /*  Line control              */
#define MCR 4 /*  Modem control             */
#define LSR 5 /*  Line Status               */
#define MSR 6 /*  Modem Status              */
#define DLL 0 /*  Divisor Latch Low         */
#define DLH 1 /*  Divisor latch High        */

#define DLAB 0x80

int early_serial_init(struct early_serial_dev *dev, uint32_t port, uint32_t divisor) {
    if(!dev)
        return EINVAL;

    memset(dev, 0, sizeof(struct early_serial_dev));

    outb(3, port + LCR);
    outb(0, port + IER);
    outb(0, port + FCR);
    outb(3, port + MCR);

    uint32_t baud = DEFAULT_BAUD / divisor;
    uint8_t c = inb(port + LCR);

    outb(c | DLAB, port + LCR);
    outb(baud & 0xff, port + DLL);
    outb((baud >> 8) & 0xff, port + DLH);
    outb(c & ~DLAB, port + LCR);

    dev->port = port;
    dev->baud = baud;

    return 0;
}

static __always_inline int __unsafe_putchar(int port, int ch) {
    for(uint32_t timeout = 0xffff; inb(port + LSR) == 0; timeout--) {
        if(!timeout)
            return ETIMEDOUT;
        rep_nop();
    }

    outb(port + TXR, ch);
    return 0;
}

int early_serial_putchar(struct early_serial_dev *dev, int ch) {
    if(!dev)
        return EINVAL;

    return __unsafe_putchar(dev->port, ch);
}

ssize_t early_serial_write(struct early_serial_dev *dev, const uint8_t *data, size_t size) {
    if(!dev || (!data && size))
        return 0;

    for(size_t i = 0; i < size; i++) {
        if(__unsafe_putchar(dev->port, data[i]))
            return i;
    }

    return size;
}

ssize_t early_serial_puts(struct early_serial_dev *dev, const char *str) {
    static_assert(sizeof(char) == sizeof(uint8_t));
    return early_serial_write(dev, (const uint8_t*) str, strlen(str));
}

