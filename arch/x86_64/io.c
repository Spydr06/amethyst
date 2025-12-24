#include <arch/x86_64/io.h>

uint8_t inb(io_port_t port) {
    uint8_t value;
    __asm__ volatile(
        "inb %w1, %b0"
        :"=a"(value)
        :"d"(port)
    );
    return value;
}

void outb(io_port_t port, uint8_t value) {
    __asm__ volatile(
        "outb %b0, %w1"
        :
        :"a"(value), "d"(port)
    );
}

uint16_t inw(io_port_t port) {
    uint16_t value;
    __asm__ volatile(
        "inw %w1, %w0"
        :"=a"(value)
        :"d"(port)
    );
    return value;
}

void outw(io_port_t port, uint16_t value) {
    __asm__ volatile(
        "outw %w0, %w1"
        :
        :"a"(value), "d"(port)
    );
}

uint32_t inl(io_port_t port) {
    uint32_t value;
    __asm__ volatile(
        "inl %w1, %0"
        :"=a"(value)
        :"d"(port)
    );
    return value;
}

void outl(io_port_t port, uint32_t value) {
    __asm__ volatile(
        "outl %0, %w1"
        :
        :"a"(value), "d"(port)
    );
}

