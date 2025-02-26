#include "x86_64/cpu/idt.h"
#include <drivers/char/ps2.h>

#include <x86_64/cpu/idt.h>
#include <x86_64/dev/apic.h>
#include <x86_64/dev/io.h>

#include <assert.h>
#include <kernelio.h>

static void mouse_isr(struct cpu_context* ctx) {
    uint8_t data = inb(PS2_PORT_DATA);
    here();
}

void ps2_mouse_init(void) {
    struct isr* isr = interrupt_allocate(mouse_isr, apic_send_eoi, IPL_MOUSE);
    assert(isr);
    io_apic_register_interrupt(MOUSE_INTERRUPT, isr->id & 0xff, _cpu()->id, false);
    klog(INFO, "PS/2 mouse interrupt enabled with vector %lu", isr->id & 0xff);
}

