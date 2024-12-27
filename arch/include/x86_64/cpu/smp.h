#ifndef _AMETHYST_X86_64_CPU_SMP_H
#define _AMETHYST_X86_64_CPU_SMP_H

#include <cpu/cpu.h>

#include <stddef.h>

enum smp_ipi_target {
    SMP_IPI_TARGET,
    SMP_IPI_SELF,
    SMP_IPI_ALL,
    SMP_IPI_OTHERCPUS
};

extern size_t smp_cpus_awake;

void smp_init(void);

void smp_send_ipi(struct cpu* cpu, struct isr* isr, enum smp_ipi_target target, bool nmi);

#endif /* _AMETHYST_X86_64_CPU_SMP_H */

