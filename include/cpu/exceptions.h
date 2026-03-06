#ifndef _AMETHYST_CPU_EXCEPTIONS_H
#define _AMETHYST_CPU_EXCEPTIONS_H

#include <cpu/cpu.h>
#include <stdint.h>

typedef void (*ex_handler_t)(struct cpu_context*, void*);

enum ex_vector : uint8_t {
    EX_DIVISION_ERROR = 0x00,
    EX_DEBUG,
    EX_NMI,
    EX_BREAKPOINT,
    EX_OVERFLOW,
    EX_BOUND_RANGE,
    EX_INVALID_OPCODE,
    EX_DEVICE_NAVAIL,
    EX_DOUBLE_FAULT,
    EX_INVALID_TSS = 0x0a,
    EX_SEGMENT,
    EX_STACK_SEGMENT_FAULT,
    EX_PROTECTION_FAULT,
    EX_PAGE_FAULT,
    EX_X87_FPE = 0x10,
    EX_ALIGNMENT_CHECK,
    EX_MACHINE_CHECK,
    EX_SIMD_FPE,
};

void pagefault_interrupt(struct cpu_context *, void*);
void protectionfault_interrupt(struct cpu_context *, void *);
void x87_fpe_interrupt(struct cpu_context *, void *);
void invalid_opcode_interrupt(struct cpu_context *, void *);

void exception_handlers_init(void);

#endif /* _AMETHYST_CPU_EXCEPTIONS_H */
