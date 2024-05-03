#ifndef _AMETHYST_X86_64_IDT_H
#define _AMETHYST_X86_64_IDT_H

#include <stdint.h>

#define IDT_PRESENT_FLAG 0x80
#define IDT_INTERRUPT_TYPE_FLAG 0x0E
#define IDT_SEGMENT_SELECTOR 0x08
#define IDT_DPL_USER_FLAG 0x60

#define DIVIDE_ERROR 0
#define DEBUG_EXC 1
#define NMI_INTERRUPT 2
#define BREAKPOINT 3
#define OVERFLOW 4
#define BOUND_RANGE_EXCEED 5
#define INVALID_OPCODE 6
#define DEV_NOT_AVL 7
#define DOUBLE_FAULT 8
#define COPROC_SEG_OVERRUN 9
#define INVALID_TSS 10
#define SEGMENT_NOT_PRESENT 11
#define STACK_SEGMENT_FAULT 12
#define GENERAL_PROTECTION 13
#define PAGE_FAULT 14
#define INT_RSV 15
#define FLOATING_POINT_ERR 16
#define ALIGNMENT_CHECK 17
#define MACHINE_CHECK 18
#define SIMD_FP_EXC 19

#define APIC_TIMER_INTERRUPT 32
#define APIC_SPURIOUS_INTERRUPT 255

#define PIT_INTERRUPT      (APIC_TIMER_INTERRUPT + 0)
#define KEYBOARD_INTERRUPT (APIC_TIMER_INTERRUPT + 1)

#define SYSCALL_INTERRUPT 0x80

struct interrupt_descriptor {
    uint16_t offset_1;
    uint16_t selector;
    uint8_t ist;
    uint8_t type_attributes;
    uint16_t offset_2;
#ifdef __x86_64__ 
    uint32_t offset_3;
    uint32_t __zero; // reserved
#endif
} __attribute__((packed));

struct idtr {
    uint16_t size;
    uintptr_t idt;
} __attribute__((packed));

struct interrupt_frame {

} __attribute__((packed));

extern void* isr_stub_table[0x100];

void init_interrupts(void);

void idt_set_descriptor(uint8_t vector, void* isr, uint8_t flags);

#endif /* _AMETHYST_X86_64_IDT_H */

