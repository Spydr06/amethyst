#ifndef _AMETHYST_SYS_SCHEDULER
#define _AMETHYST_SYS_SCHEDULER

#include <stddef.h>
#include <cdefs.h>

#include <filesystem/virtual.h>

#define PROC_HOLD(v) (__atomic_add_fetch(&(v)->ref_count, 1, __ATOMIC_SEQ_CST))

struct proc {
    int ref_count;
    struct cred cred;
};

void scheduler_init(void);
void scheduler_apentry(void);

struct thread* sched_new_thread(void* ip, size_t kernel_stack_size, int priority, struct proc* proc, void* user_stack);

#endif /* _AMETHYST_SYS_SCHEDULER */

