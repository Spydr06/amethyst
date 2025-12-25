#ifndef _AMETHYST_X86_64_IO_H
#define _AMETHYST_X86_64_IO_H

#include <stdint.h>

typedef uint16_t io_port_t;

uint8_t inb(io_port_t port);
void outb(io_port_t port, uint8_t value);

uint16_t inw(io_port_t port);
void outw(io_port_t port, uint16_t value);

uint32_t inl(io_port_t port);
void outl(io_port_t port, uint32_t value);

#endif /* _AMETHYST_X86_64_IO_H */

