#ifndef _AMETHYST_X86_64_CPU_H
#define _AMETHYST_X86_64_CPU_H

#include <stdint.h>
#include <cdefs.h>

#include "msr.h"
#include "gdt.h"
#include "idt.h"

#define CPU_SP(ctx) ((ctx)->rsp)
#define CPU_IP(ctx) ((ctx)->rip)

typedef uint64_t register_t;

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
    unsigned cpu_num;

    struct interrupt_handler interrupt_handlers[0x100];
    
    struct thread* idle_thread;

    void* scheduler_stack;

    struct cpu_features features;
    struct cpu* this;
};

struct cpu_context {
    uint64_t cr2;
	uint64_t gs;
	uint64_t fs;
	uint64_t es;
	uint64_t ds;
	uint64_t rax;
	uint64_t rbx;
	uint64_t rcx;
	uint64_t rdx;
	uint64_t r8;
	uint64_t r9;
	uint64_t r10;
	uint64_t r11;
	uint64_t r12;
	uint64_t r13;
	uint64_t r14;
	uint64_t r15;
	uint64_t rdi;
	uint64_t rsi;
	uint64_t rbp;

	uint64_t error_code;
	
    uint64_t rip;
	uint64_t cs;
	uint64_t rflags;
	uint64_t rsp;
	uint64_t ss;
} __attribute__((__packed__));

struct cpu_extra_context {
    uint64_t gs_base;
    uint64_t fs_base;
    __attribute__((aligned(16))) uint8_t fx[512];
    uint32_t mxcsr;
} __attribute__((packed));

extern void _context_saveandcall(void (*fn)(struct cpu_context*), void* stack);
extern void _context_switch(struct cpu_context* ctx);

static __always_inline void cpu_ctx_init(struct cpu_context* ctx, bool u, bool interrupts) {
    if(u) {
        ctx->cs = 0x23;
        ctx->ds = ctx->es = ctx->ss = 0x1b;
    }
    else {
        ctx->cs = 0x8;
        ctx->ds = ctx->es = ctx->ss = 0x10;
    }
    ctx->rflags = interrupts ? 0x200 : 0;
}

static __always_inline void cpu_extra_ctx_init(struct cpu_extra_context* ctx) {
    // init x87 FPU state
    ctx->mxcsr = 0x1f80;
    *((uint16_t*) &ctx->fx[0]) = 0x37f;
    *((uint16_t*) &ctx->fx[2]) = 0;
    ctx->fx[4] = 0;
}

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

