#ifndef _AMETHYST_X86_64_TRACE_H
#define _AMETHYST_X86_64_TRACE_H

#include <stdint.h>
#include <limine.h>
#include <cpu/cpu.h>

void dump_stack(void);
void dump_registers(struct cpu_context* ctx);

#endif /* _AMETHYST_X86_64_TRACE_H */

