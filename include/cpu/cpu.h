#ifndef _AMETHYST_CPU_H
#define _AMETHYST_CPU_H

#if defined(__x86_64__)
    #include <x86_64/cpu/cpu.h>
#endif

void cpu_enable_features(void);

#endif /* _AMETHYST_CPU_H */

