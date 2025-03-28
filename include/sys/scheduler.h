#ifndef _AMETHYST_SYS_SCHEDULER
#define _AMETHYST_SYS_SCHEDULER

#include <stddef.h>
#include <cdefs.h>

#include <sys/thread.h>
#include <sys/syscall.h>

void scheduler_init(void);
void scheduler_apentry(void);
__noreturn void sched_stop_thread(void);

void sched_pin(cpuid_t pin);
void sched_sleep(size_t us);
void sched_prepare_sleep(bool interruptible);
bool sched_wakeup(struct thread* thread, enum wakeup_reason reason);

int sched_queue(struct thread* thread);
int sched_yield(void);

int scheduler_exec(const char* path, char* argv[], char* envp[]);
void scheduler_terminate(int status);

void sched_stop_other_threads(void);

__syscall void _sched_userspace_check(struct cpu_context* context, bool syscall, uint64_t syscall_errno, uint64_t syscall_ret);

#endif /* _AMETHYST_SYS_SCHEDULER */

