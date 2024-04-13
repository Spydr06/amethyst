#include "idt.h"

#include <arch/x86_64/processor.h>

#include <stddef.h>
#include <cdefs.h>
#include <stdint.h>

#include <kernelio.h>

__aligned(0x10) struct interrupt_descriptor idt[256];
const struct idtr idtr = {
    .size = sizeof(idt) - 1,
    .idt = (uintptr_t) (void*) idt
};

static const char *exception_names[] = {
  "Divide by Zero Error",
  "Debug",
  "Non Maskable Interrupt",
  "Breakpoint",
  "Overflow",
  "Bound Range",
  "Invalid Opcode",
  "Device Not Available",
  "Double Fault",
  "Coprocessor Segment Overrun",
  "Invalid TSS",
  "Segment Not Present",
  "Stack-Segment Fault",
  "General Protection Fault",
  "Page Fault",
  "Reserved",
  "x87 Floating-Point Exception",
  "Alignment Check",
  "Machine Check",
  "SIMD Floating-Point Exception",
  "Reserved",
  "Reserved",
  "Reserved",
  "Reserved",
  "Reserved",
  "Reserved",
  "Reserved",
  "Reserved",
  "Reserved",
  "Reserved",
  "Security Exception",
  "Reserved"
};


void idt_set_descriptor(uint8_t vector, void* isr, uint8_t flags) {
    idt[vector].offset_1 = (uintptr_t) isr & 0xffff;
    idt[vector].offset_2 = ((uintptr_t) isr >> 16) & 0xffff;
#ifdef __x86_64__
    idt[vector].offset_3 = ((uintptr_t) isr >> 32) & 0xffffffff;
#endif
    idt[vector].selector = IDT_SEGMENT_SELECTOR;
    idt[vector].type_attributes = flags;
}

void init_interrupts(void) {
    extern void* isr_stub_table[];
    for(uint8_t i = 0; i <= 18; i++) {
        idt_set_descriptor(i, isr_stub_table[i], IDT_PRESENT_FLAG | IDT_INTERRUPT_TYPE_FLAG);
    }
    for(uint8_t i = 0; i <= 34; i++) {
        idt_set_descriptor(i, isr_stub_table[i], IDT_PRESENT_FLAG | IDT_INTERRUPT_TYPE_FLAG);
    }
    idt_set_descriptor(255, isr_stub_table[255], IDT_PRESENT_FLAG | IDT_INTERRUPT_TYPE_FLAG);
    idt_reload();
}

void idt_reload(void) {
    __asm__ volatile (
        "cli;"
        "lidt %0;"
        "sti"
        :: "m"(idtr)
    );
}

cpu_status_t* __interrupt_handler(cpu_status_t* status) {
    switch(status->interrupt_number) {
        case DOUBLE_FAULT:
            klog(ERROR, "Double Fault at %p.", (void*) status->error_code);
            break;
        default:
            if(status->interrupt_number < __len(exception_names))
                panic("Unhandled Interrupt \"%s\".", exception_names[status->interrupt_number]);
            else
                panic("Unhandled Interrupt %llu.", (unsigned long long) status->interrupt_number);
    }

    return status; 
}

