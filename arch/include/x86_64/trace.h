#ifndef _AMETHYST_X86_64_TRACE_H
#define _AMETHYST_X86_64_TRACE_H

#include <stdint.h>
#include <limine/limine.h>
#include <cpu/cpu.h>

void load_symtab(struct limine_kernel_file_response* response);
const char* symtab_lookup(uintptr_t address);

void dump_stack(void);
void dump_registers(struct cpu_context* ctx);

#endif /* _AMETHYST_X86_64_TRACE_H */

