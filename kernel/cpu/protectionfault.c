#include <cpu/cpu.h>
#include <cpu/interrupts.h>
#include <mem/vmm.h>
#include <sys/thread.h>

static void protectionfault_interrupt(struct cpu_context* status) {
    struct thread* thread = current_thread();

    interrupt_set(true);

    bool in_userspace = status->cs != 8;
}

void protectionfault_init(void) {
    interrupt_register(0x0d, protectionfault_interrupt, nullptr, IPL_IGNORE);
}
