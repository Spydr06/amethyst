#ifndef _AMETHYST_X86_64_IO_H
#define _AMETHYST_X86_64_IO_H

#include <stdint.h>

uint8_t inb(uint16_t port);
void outb(uint16_t port, uint8_t value);

uint16_t inw(uint16_t port);
void outw(uint16_t port, uint16_t value);

uint32_t inl(uint16_t port);
void outl(uint16_t port, uint32_t value);

#endif /* _AMETHYST_X86_64_IO_H */

