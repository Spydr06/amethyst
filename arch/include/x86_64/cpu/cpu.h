#ifndef _AMETHYST_X86_64_CPU_H
#define _AMETHYST_X86_64_CPU_H

#include <stdint.h>
#include <cdefs.h>

#include "msr.h"
#include "gdt.h"

#include <scheduler/thread.h>

struct ist {
    uint32_t reserved;
	uint64_t rsp0;
	uint64_t rsp1;
	uint64_t rsp2;
	uint64_t reserved2;
	uint64_t ist1;
	uint64_t ist2;
	uint64_t ist3;
	uint64_t ist4;
	uint64_t ist5;
	uint64_t ist6;
	uint64_t ist7;
	uint32_t reserved3[3];
	uint32_t iopb;
};

struct cpu_features {
    bool sse_supported : 1;
    bool sse2_supported : 1;
    /*bool sse3_supported : 1;
    bool sse4_1_supported : 1;
    bool sse4_2_supported : 1;
    bool sse4_a_supported : 1;*/

    bool syscall_supported : 1;
    bool x87_fpu_supported : 1;
};

struct cpu {
    struct thread* thread;

    gdte_t gdt[GDT_NUM_ENTRIES];
    struct ist ist;
    struct vmm_context* vmm_context;

    bool interrupt_status;

    struct cpu_features features;
    struct cpu* this;
};

typedef struct {
    uint64_t r15;
    uint64_t r14;
    uint64_t r13;
    uint64_t r12;
    uint64_t r11;
    uint64_t r10;
    uint64_t r9;
    uint64_t r8;
    uint64_t rdi;
    uint64_t rsi;
    uint64_t rbp;
    uint64_t rdx;
    uint64_t rcx;
    uint64_t rbx;
    uint64_t rax;
    
    uint64_t interrupt_number;
    uint64_t error_code;
    
    uint64_t rip;
    uint64_t cs;
    uint64_t rflags;
    uint64_t rsp;
    uint64_t ss;
} __attribute__((__packed__)) cpu_status_t;

extern void _context_saveandcall(void (*fn)(cpu_status_t*), void* stack);
extern void _context_switch(cpu_status_t* status);

static __always_inline void cpu_set(struct cpu* ptr) {
    ptr->this = ptr;
    wrmsr(MSR_GSBASE, (uintptr_t) &ptr->this);
}

static __always_inline struct cpu* _cpu(void) {
    volatile struct cpu* cpu;
    __asm__ volatile (
        "mov %%gs:0, %%rax"
        : "=a"(cpu)
    );
    return cpu->this;
}

_Noreturn static __always_inline void hlt(void) {
    __asm__ volatile("cli; hlt");
    while(1); // unreachable
}

static __always_inline void rep_nop(void) {
    __asm__ volatile("rep; nop" ::: "memory");
}

static __always_inline uint16_t ds(void)
{
	uint16_t seg;
	__asm__ volatile("movw %%ds,%0" : "=rm" (seg));
	return seg;
}

static __always_inline uint16_t fs(void)
{
	uint16_t seg;
	__asm__ volatile("movw %%fs,%0" : "=rm" (seg));
	return seg;
}

static __always_inline uint16_t gs(void)
{
	uint16_t seg;
	__asm__ volatile("movw %%gs,%0" : "=rm" (seg));
	return seg;
}

static __always_inline void pause(void) {
    __asm__ volatile("pause");
}

#endif /* _AMETHYST_X86_64_CPU_H */

