#include <x86_64/init/early_serial.h>

#include <x86_64/dev/io.h>
#include <x86_64/cpu/cpu.h>

#include <cdefs.h>

#define DEFAULT_SERIAL_PORT 0x3f8

#define DLAB 0x80

#define DEFAULT_BAUD 9600

int32_t early_serial_base __section(".data") = 0;

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

void early_console_init(void)
{
    early_serial_init(DEFAULT_SERIAL_PORT, DEFAULT_BAUD);
}

static void serial_putchar(int ch) {
    uint32_t timeout = 0xffff;

    while((inb(early_serial_base + LSR)) == 0 && --timeout)
        rep_nop();

    outb(early_serial_base + TXR, ch);
}

void early_putchar(int ch) {
    if(ch == '\n')
        early_putchar('\r');

    if(early_serial_base != 0)
        serial_putchar(ch);
}
