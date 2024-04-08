#include <arch/x86-common/io.h>

void putb(uint8_t c) {
    while((inb(0x3f8+5) & 0x20) == 0);

    outb(0x3f8, c);
}

void puts(const char* s) {
    while(*s++)
        putb(*s);
}

void kmain(void)
{
    puts("Hello, World!\n");
    while(1);
}
