#include <x86_64/trace.h>
#include <encoding/elf.h>

#include <kernelio.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

struct stackframe {
    struct stackframe* rbp; // ebp on x86
    uintptr_t rip; // eip on x86
};

void __attribute__((no_sanitize("undefined"))) 
dump_stack(void) {
    struct stackframe* stack;
    stack = (struct stackframe*) __builtin_frame_address(0);

    for(size_t frame = 0; stack && frame < 4096; frame++) {
        if(stack->rip) {
            if(kernel_elf_inited()) {
                const char* symbol = kernel_lookup_symbol(stack->rip);
                if(!symbol)
                    symbol = "...";
                printk("  0x%llu  [%s]\n", (unsigned long long) stack->rip, symbol);
            }
            else
                printk("  0x%llu\n", (unsigned long long) stack->rip);
        }
        stack = stack->rbp;
    }
}

void dump_registers(struct cpu_context* ctx) {
    printk("    rax = %lx, rbx = %lx, rcx = %lx, rdx = %lx\n", ctx->rax, ctx->rbx, ctx->rcx, ctx->rdx);
    printk("    r8 = %lx, r9 = %lx, r10 = %lx, r11 = %lx\n", ctx->r8, ctx->r9, ctx->r10, ctx->r11);
    printk("    r12 = %lx, r13 = %lx, r14 = %lx, r15 = %lx\n", ctx->r12, ctx->r13, ctx->r14, ctx->r15);
    printk("    gs = %lx, fs = %lx, es = %lx, ds = %lx\n", ctx->gs, ctx->fs, ctx->es, ctx->ds);
    printk("    cs = %lx, rflags = %lx,  ss = %lx\n", ctx->cs, ctx->rflags, ctx->ss);
    printk("    rip = %lx, rsp = %lx, rbp = %lx \n    error_code = %lx, cr2 = %lx\n", ctx->rip, ctx->rsp, ctx->rbp, ctx->error_code, ctx->cr2);
}
