#include "boot.h"
#include "../io.h"

#define DEFAULT_SERIAL_PORT 0x3f8

#define DLAB 0x80

#define DEFAULT_BAUD 9600

static void early_serial_init(int port, int baud) {
    outb(3, port + LCR);
    outb(0, port + IER);
    outb(0, port + FCR);
    outb(3, port + MCR);

    uint32_t divisor = 115200 / baud;
    uint8_t c = inb(port + LCR);

    outb(c | DLAB, port + LCR);
    outb(divisor & 0xff, port + DLL);
    outb((divisor >> 8) & 0xff, port + DLH);
    outb(c & ~DLAB, port + LCR);

    early_serial_base = port;
}

void console_init(void)
{
    early_serial_init(DEFAULT_SERIAL_PORT, DEFAULT_BAUD);
}
