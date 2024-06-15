#include <x86_64/cpu/idt.h>
#include <x86_64/dev/pic.h>
#include <x86_64/cpu/cpu.h>
#include <cpu/syscalls.h>
#include <drivers/char/keyboard.h>

#include <cpu/cpu.h>
#include <mem/vmm.h>
#include <cdefs.h>
#include <string.h>
#include <stdint.h>

#include <kernelio.h>

extern void _idt_reload(volatile const struct idtr* idtr);
extern void _interrupt_enable(void);
extern void _interrupt_disable(void);

//#define DOPENING_SAVE() _context_saveandcall(dopening, nullptr)

extern uint64_t _millis;

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

static void load_default(void);

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
    load_default();
    
    interrupts_apinit();
}

void interrupts_apinit(void) {
    load_default();

    _idt_reload(&idtr);
    interrupt_set(true);
}

void idt_reload(void) {
    _idt_reload(&idtr);
}

static void tick(struct cpu_context* status) {
    _millis++;
}

static void double_fault_handler(struct cpu_context* status) {
    klog(ERROR, "Double Fault at %p.", (void*) status->error_code);
}

static void divide_error_handler(struct cpu_context* status) {
    panic("Division by Zero (%lu).", status->error_code);
}

static void load_default(void) {
    struct interrupt_handler* handlers = _cpu()->interrupt_handlers;
    memset(handlers, 0, sizeof(struct interrupt_handler) * 0x100);

//    [KEYBOARD_INTERRUPT] = {keyboard_interrupt_handler, pic_send_eoi},

    handlers[PIT_INTERRUPT] = (struct interrupt_handler){tick, pic_send_eoi};
    handlers[PAGE_FAULT] = (struct interrupt_handler){page_fault_handler, nullptr};
    handlers[DOUBLE_FAULT] = (struct interrupt_handler){double_fault_handler, nullptr};
    handlers[DIVIDE_ERROR] = (struct interrupt_handler){divide_error_handler, nullptr};
}

void idt_register_interrupt(uint8_t vector, void (*handler)(struct cpu_context*), void (*eoi_handler)(uint32_t)) {
    _cpu()->interrupt_handlers[vector].handler = handler;
    _cpu()->interrupt_handlers[vector].eoi_handler = eoi_handler;
}

void __interrupt_handler(struct cpu_context* status, uint64_t interrupt_number) {
    if(_cpu()->interrupt_handlers[interrupt_number].handler) {
        _cpu()->interrupt_handlers[interrupt_number].handler(status);
        if(_cpu()->interrupt_handlers[interrupt_number].eoi_handler)
            _cpu()->interrupt_handlers[interrupt_number].eoi_handler(interrupt_number - APIC_TIMER_INTERRUPT);
        return;
    }

    if(interrupt_number < __len(exception_names))
        panic("Unhandled Interrupt %llu: %s.", (unsigned long long) interrupt_number, exception_names[interrupt_number]);
    else
        panic("Unhandled Interrupt %llu.", (unsigned long long) interrupt_number);

    return; 
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

void idt_change_eoi(void (*eoi_handler)(uint32_t isr)) {
    bool before = interrupt_set(false);
       
    struct interrupt_handler* handlers = _cpu()->interrupt_handlers;

    for(size_t i = 0; i < 0x100; i++) {
        if(handlers[i].eoi_handler)
            handlers[i].eoi_handler = eoi_handler;
    }

    interrupt_set(before);
}
