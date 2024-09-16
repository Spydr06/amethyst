#ifndef _AMETHYST_X86_64_CPU_H
#define _AMETHYST_X86_64_CPU_H

#include <stdint.h>
#include <cdefs.h>

#include <sys/timer.h>

#include "msr.h"
#include "gdt.h"
#include "idt.h"

#define CPU_SP(ctx) ((ctx)->rsp)
#define CPU_IP(ctx) ((ctx)->rip)
#define CPU_RET(ctx) ((ctx)->rax)

#define CPU_CONTEXT_INTSTATUS(ctx) ((bool) ((ctx)->rflags & 0x200))

#define CPU_CONTEXT_THREADSAVE(t, c) do {                                        \
        memcpy(&(t)->context, c, sizeof(struct cpu_context));                    \
        (t)->extra_context.gs_base = rdmsr(MSR_KERNELGSBASE);                    \
	    (t)->extra_context.fs_base = rdmsr(MSR_FSBASE);                          \
	    __asm__ volatile ("fxsave (%%rax)" :: "a"(&(t)->extra_context.fx[0]));   \
	    __asm__ volatile ("stmxcsr (%%rax)" :: "a"(&(t)->extra_context.mxcsr));  \
    } while(0)

#define CPU_CONTEXT_SWITCHTHREAD(t) do {                                        \
        _cpu()->ist.rsp0 = (uint64_t)(t)->kernel_stack_top;                     \
        wrmsr(MSR_KERNELGSBASE, (t)->extra_context.gs_base);                    \
        wrmsr(MSR_FSBASE, (t)->extra_context.fs_base);                          \
        __asm__ volatile ("fxrstor (%%rax)" :: "a"(&(t)->extra_context.fx[0])); \
        __asm__ volatile ("ldmxcsr (%%rax)" :: "a"(&(t)->extra_context.mxcsr)); \
        _context_switch(&(t)->context);                                         \
    } while(0)

typedef uint64_t register_t;

typedef int16_t cpuid_t;

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
	uint32_t reserved3[2];
	uint32_t iopb;
} __attribute__((packed));

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
    cpuid_t id;
    cpuid_t acpiid;
    int _errno;

    enum ipl ipl;
    struct isr isr[0x100];
    struct isr* isr_queue;

    struct isr* dpc_isr;
    struct dpc* dpc_queue;
    
    struct thread* idle_thread;

    struct timer_entry sched_timer_entry;
    void* scheduler_stack;

    struct cpu_features features;
    struct cpu* this;

    struct timer* timer;
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

extern int _context_save_and_call(void (*fn)(struct cpu_context*, void*), void* stack, void* userp);
extern __noreturn void _context_switch(struct cpu_context* ctx);

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

static __always_inline
#ifdef _AMETHYST_CPU_SYSCALLS_H
    __no_caller_saved_registers __general_regs_only
#endif
bool cpu_ctx_is_user(struct cpu_context* ctx) {
    return ctx->cs == 0x23;
}

static __always_inline void cpu_set(struct cpu* ptr) {
    ptr->this = ptr;
    wrmsr(MSR_GSBASE, (uintptr_t) &ptr->this);
}

static __always_inline 
#ifdef _AMETHYST_CPU_SYSCALLS_H
    __no_caller_saved_registers __general_regs_only
#endif
struct cpu* _cpu(void) {
    volatile struct cpu* cpu;
    __asm__ volatile (
        "mov %%gs:0, %%rax"
        : "=a"(cpu)
    );
    return cpu->this;
}

static __always_inline void hlt_until_int(void) {
    __asm__ volatile("hlt");
}

__noreturn static __always_inline void hlt(void) {
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

