#include <x86_64/cpu/idt.h>
#include <x86_64/dev/pic.h>
#include <x86_64/cpu/cpu.h>
#include <cpu/syscalls.h>
#include <drivers/char/keyboard.h>

#include <cpu/cpu.h>
#include <mem/vmm.h>
#include <cdefs.h>
#include <stdint.h>

#include <kernelio.h>

extern void _idt_reload(volatile const struct idtr* idtr);
extern void _interrupt_enable(void);
extern void _interrupt_disable(void);

//#define DOPENING_SAVE() _context_saveandcall(dopening, nullptr)

extern uint64_t __millis;

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
    _idt_reload(&idtr);
    interrupt_set(true);
}

void idt_reload(void) {
    _idt_reload(&idtr);
}

static cpu_status_t* tick(cpu_status_t* status) {
    __millis++;
    return status;
}

static cpu_status_t* double_fault_handler(cpu_status_t* status) {
    klog(ERROR, "Double Fault at %p.", (void*) status->error_code);
    return status;
}

static cpu_status_t* divide_error_handler(cpu_status_t* status) {
    panic("Division by Zero (%lu).", status->error_code);
}

static struct {
    cpu_status_t* (*handler)(cpu_status_t*);
    void (*eoi_handler)(uint32_t);
} interrupt_handlers[UINT8_MAX] = {
    [PIT_INTERRUPT]      = {tick,                       pic_send_eoi},
    [KEYBOARD_INTERRUPT] = {keyboard_interrupt_handler, pic_send_eoi},
    [PAGE_FAULT]         = {page_fault_handler,         nullptr     },
    [DOUBLE_FAULT]       = {double_fault_handler,       nullptr     },
    [DIVIDE_ERROR]       = {divide_error_handler,       nullptr     },
//    [SYSCALL_INTERRUPT]  = {syscall_dispatch,           nullptr     },
};

void idt_register_interrupt(uint8_t vector, cpu_status_t* (*handler)(cpu_status_t*), void (*eoi_handler)(uint32_t)) {
    interrupt_handlers[vector].handler = handler;
    interrupt_handlers[vector].eoi_handler = eoi_handler;
}

cpu_status_t* __interrupt_handler(cpu_status_t* status) {
    if(interrupt_handlers[status->interrupt_number].handler) {
        cpu_status_t* new_status = interrupt_handlers[status->interrupt_number].handler(status);
        if(interrupt_handlers[status->interrupt_number].eoi_handler)
            interrupt_handlers[status->interrupt_number].eoi_handler(status->interrupt_number - APIC_TIMER_INTERRUPT);
        return new_status;
    }

    if(status->interrupt_number < __len(exception_names))
        panic("Unhandled Interrupt \"%s\".", exception_names[status->interrupt_number]);
    else
        panic("Unhandled Interrupt %llu.", (unsigned long long) status->interrupt_number);

    return status; 
}

bool interrupt_set(bool status) {
    bool old = _cpu()->interrupt_status;
    _cpu()->interrupt_status = status;

    if(status)
        _interrupt_enable();
    else
        _interrupt_disable();

    // TODO:
    // if(status)
    //     DOPENING_SAVE();

    return old;
}

