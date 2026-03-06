#include <cpu/exceptions.h>

#include <cdefs.h>

void exception_handlers_init(void) {
    static const ex_handler_t handlers[] = {
        [EX_PAGE_FAULT] = pagefault_interrupt,
        [EX_PROTECTION_FAULT] = protectionfault_interrupt,
        [EX_X87_FPE] = x87_fpe_interrupt,
        [EX_INVALID_OPCODE] = invalid_opcode_interrupt,
    };

    for(uint8_t vector = 0; vector < __len(handlers); vector++) {
        if(!vector[handlers])
            continue;
        interrupt_register(vector, handlers[vector], nullptr, IPL_IGNORE);
    }
}
