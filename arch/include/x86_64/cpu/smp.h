#ifndef _AMETHYST_X86_64_CPU_SMP_H
#define _AMETHYST_X86_64_CPU_SMP_H

#include <stddef.h>
#include <cdefs.h>

#include "idt.h"

struct cpu;

enum smp_ipi_target {
    SMP_IPI_TARGET,
    SMP_IPI_SELF,
    SMP_IPI_ALL,
    SMP_IPI_OTHERCPUS
};

extern volatile size_t smp_cpus_awake;

void smp_init(void);

void smp_send_ipi(struct cpu* cpu, struct isr* isr, enum smp_ipi_target target, bool nmi);

__noreturn void smp_hlt(void);

struct cpu* smp_get_cpu(unsigned smp_id);

#endif /* _AMETHYST_X86_64_CPU_SMP_H */

