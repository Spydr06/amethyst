#include <kernelio.h>

#include <stddef.h>
#include <stdint.h>

struct stackframe {
    struct stackframe* rbp; // ebp on x86
    uintptr_t rip; // eip on x86
};

void dump_stack(void) {
    struct stackframe* stack;
    stack = (struct stackframe*) __builtin_frame_address(0);

    for(size_t frame = 0; stack && frame < 4096; frame++) {
        if(stack->rip)
            printk("  0x%llu\n", (unsigned long long) stack->rip);
        stack = stack->rbp;
    }
}

