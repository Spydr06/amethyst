#include "sys/scheduler.h"
#include "sys/signal.h"
#include <cpu/exceptions.h>

#include <amethyst/signal.h>
#include <cpu/cpu.h>
#include <cpu/interrupts.h>
#include <mem/vmm.h>
#include <sys/thread.h>

#include <assert.h>
#include <kernelio.h>

static void signal_fault(signo_t signo, uintptr_t addr) {
    assert(current_thread() != nullptr);
    struct siginfo siginfo = {
        .si_signo = signo,
        .si_attrs.fault = { .addr = addr },
    };
    signal_thread(current_thread(), &siginfo);
}

void protectionfault_interrupt(struct cpu_context *status, void *) {
    if(cpu_ctx_is_user(status)) {
        signal_fault(SIGSEGV, status->rip);
        return;
    }

    panic("protection fault");
}

void x87_fpe_interrupt(struct cpu_context *status, void *) {
    if(cpu_ctx_is_user(status)) {
        signal_fault(SIGFPE, status->rip);
        return;
    }

    panic("x87 floating-point exception");
}

void invalid_opcode_interrupt(struct cpu_context *status, void *) {
    if(cpu_ctx_is_user(status)) {
        signal_fault(SIGILL, status->rip);
        return;
    }

    panic("x87 floating-point exception");
}