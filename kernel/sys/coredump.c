#include <sys/coredump.h>
#include <sys/spinlock.h>

#include <amethyst/amethyst.h>

static spinlock_t core_dump_lock = SPINLOCK_INIT;

void core_dump(struct proc *proc, struct siginfo *info) {
    spinlock_acquire(&core_dump_lock);
    klog(INFO, "+++ Core dump for process [pid %d] +++", proc->pid);

    if(info) {
        klog(INFO, "\tSignal: %s [%d]", strsignal(info->si_signo), info->si_signo);
        klog(INFO, "\tFault address: %p", (void*) info->si_attrs.fault.addr);
    }

    klog(INFO, "--- Core dump end ---");
    spinlock_release(&core_dump_lock);
}
