#include <drivers/char/ps2.h>

#include <x86_64/cpu/idt.h>
#include <x86_64/dev/apic.h>

#include <assert.h>
#include <kernelio.h>

static void keyboard_isr(struct cpu_context* ctx) {
    here();
}

void ps2_keyboard_init(void) {
    struct isr* isr = interrupt_allocate(keyboard_isr, apic_send_eoi, IPL_KEYBOARD);
    assert(isr);
    io_apic_register_interrupt(KEYBOARD_INTERRUPT, isr->id & 0xff, _cpu()->id, false);
    here(); 
}
