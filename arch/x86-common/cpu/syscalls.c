#include <cpu/syscalls.h>
#include "../idt.h"
#include "kernelio.h"

#define SYSCALL_IDT_VECTOR 0x80

bool syscalls_init(void)
{
    idt_set_descriptor(SYSCALL_IDT_VECTOR, isr_stub_table[SYSCALL_IDT_VECTOR], IDT_PRESENT_FLAG | IDT_INTERRUPT_TYPE_FLAG | IDT_DPL_USER_FLAG);
    klog(DEBUG, "Syscalls enabled on interrupt 0x80");
    return true;
}

cpu_status_t* syscall_dispatch(cpu_status_t* regs)
{
    uintptr_t ret = 0;
    
    switch(regs->rax) {
        default:
            klog(WARN, "unimplemented - syscall %zu\n", regs->rax);
            break;
    }

    regs->rax = ret;
    return regs;
}

size_t execute_syscall(size_t syscall_num, size_t arg0, size_t arg1, size_t arg2)
{

}

