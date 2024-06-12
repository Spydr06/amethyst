#ifndef _AMETHYST_CPU_H
#define _AMETHYST_CPU_H

#if defined(__x86_64__)
    #include <x86_64/cpu/cpu.h>
#endif

#if !defined(CPU_IP) || !defined(CPU_SP)
    #error "incomplete CPU implementation"
#endif

// each arch must define these:
struct cpu_context;
struct cpu_extra_context;
struct cpu;

void cpu_enable_features(void);

#endif /* _AMETHYST_CPU_H */

