#include <drivers/char/ps2.h>

#include <x86_64/cpu/idt.h>
#include <x86_64/dev/apic.h>
#include <x86_64/dev/io.h>

#include <assert.h>
#include <kernelio.h>

static void keyboard_isr(struct cpu_context* ctx) {
    uint8_t scancode = inb(PS2_PORT_DATA);
    here();
}

void ps2_keyboard_init(void) {
    struct isr* isr = interrupt_allocate(keyboard_isr, apic_send_eoi, IPL_KEYBOARD);
    assert(isr);
    io_apic_register_interrupt(KEYBOARD_INTERRUPT, isr->id & 0xff, _cpu()->id, false);
    klog(INFO, "PS/2 keyboard interrupt enabled with vector %lu", isr->id & 0xff);
}

