#include <sys/syscall.h>
#include <cpu/cpu.h>

#include <sys/thread.h>
#include <sys/proc.h>
#include <sys/scheduler.h>

#include <memory.h>
#include <errno.h>

__attribute__((noinline))
static void threadsave(struct thread* thread, struct cpu_context* ctx) {
    CPU_CONTEXT_THREADSAVE(thread, ctx);
}

__syscall syscallret_t _sys_fork(struct cpu_context* ctx) {
    syscallret_t ret = {
        .ret = -1,
        ._errno = 0
    };

    struct proc* proc = current_proc();
    struct thread* thread = current_thread();

    struct proc* new_proc = proc_create();
    if(!new_proc) {
        ret._errno = ENOMEM;
        goto cleanup;
    }

    uintptr_t ip = CPU_IP(ctx);
    uintptr_t sp = CPU_SP(ctx);

    struct thread* new_thread = thread_create((void*) ip, PAGE_SIZE * 16, 1, new_proc, (void*) sp);
    if(!new_thread) {
        ret._errno = ENOMEM;
        goto cleanup;
    }

    new_thread->vmm_context = vmm_context_fork(thread->vmm_context);
    if(!new_thread->vmm_context) {
        ret._errno = ENOMEM;
        goto cleanup;
    }

    if((ret._errno = fd_clone(new_proc)))
        goto cleanup;

    mutex_acquire(&proc->mutex, false);
    
    new_proc->parent = proc;
    new_proc->sibling = proc->child;
    proc->child = new_proc;

    mutex_release(&proc->mutex);

    new_proc->umask = proc->umask;
    new_proc->cred = proc->cred;
    new_proc->root = proc_get_root();
    new_proc->cwd = proc_get_cwd();
    new_thread->proc = new_proc;

    threadsave(new_thread, ctx);
    CPU_SP(&new_thread->context) = sp;
    CPU_IP(&new_thread->context) = ip;
    CPU_RET(&new_thread->context) = 0;

    // TODO: signals
    
    ret.ret = new_proc->pid;

    sched_queue(new_thread);

    klog(DEBUG, "thread [tid %d] created thread [tid %d]", thread->tid, new_thread->tid);

    PROC_RELEASE(new_proc);

cleanup:
    return ret;
}

