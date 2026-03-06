#ifndef _AMETHYST_COREDUMP_H
#define _AMETHYST_COREDUMP_H

#include <sys/proc.h>

void core_dump(struct proc *proc, struct siginfo *info);

#endif /* _AMETHYST_COREDUMP_H */
