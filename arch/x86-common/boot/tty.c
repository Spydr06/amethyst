#include "boot.h"

#include "../io.h"
#include "../processor.h"

#include <cdefs.h>

int32_t early_serial_base __section(".data") = 0;

static void serial_putchar(int ch) {
    uint32_t timeout = 0xffff;

    while((inb(early_serial_base + LSR)) == 0 && --timeout)
        rep_nop();

    outb(early_serial_base + TXR, ch);
}

void early_putchar(int ch) {
    switch(ch) {
        case '\e':
            return;
        case '\n':
            early_putchar('\r');
            break;
    }

    if(early_serial_base != 0)
        serial_putchar(ch);
}

