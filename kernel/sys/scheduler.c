#include "filesystem/device.h"
#include "filesystem/virtual.h"
#include "sys/fd.h"
#include "sys/semaphore.h"
#include "sys/syscall.h"
#include "x86_64/cpu/cpu.h"
#include "x86_64/cpu/idt.h"
#include <sys/loadavg.h>
#include <sys/scheduler.h>
#include <sys/thread.h>
#include <sys/proc.h>
#include <sys/mutex.h>
#include <sys/spinlock.h>
#include <sys/dpc.h>

#include <cpu/cpu.h>
#include <mem/slab.h>
#include <mem/vmm.h>
#include <ff/elf.h>

#include <hashtable.h>
#include <kernelio.h>
#include <assert.h>
#include <string.h>
#include <abi.h>
#include <errno.h>

//#define _SCHED_DEBUG

#define SCHEDULER_STACK_SIZE (PAGE_SIZE * 16)

#define RUN_QUEUE_COUNT ((int) sizeof(run_queue_bitmap) * 8)

struct run_queue {
    struct thread* head;
    struct thread* last;
};

struct check_args {
    struct cpu_context* context;
    bool syscall;
    uint64_t syscall_errno;
    uint64_t syscall_ret;
};

static spinlock_t run_queue_lock;
static uint64_t run_queue_bitmap;

static struct run_queue run_queue[RUN_QUEUE_COUNT];

static __noreturn void preempt_callback(void);

void sched_pin(cpuid_t pin) {
    bool interrupt_status = interrupt_set(false);
    current_thread()->pin = pin;
    interrupt_set(interrupt_status);
}

static void cpu_idle_thread(void) {
    sched_pin(_cpu()->id);

    interrupt_set(true);
    while(1) {
        hlt_until_int();
        sched_yield();
    }
}

static void print_queue(int priority) {
    if(klog_min_severity > KLOG_DEBUG)
        return;
    
    klog_inl(INFO, " %s %d | ", priority == 0 ? "->" : "  ", priority);

    struct thread* head = run_queue[priority].head;

    while(head) {
        printk(" [tid %d]", head->tid);
        head = head->next;
    }
}

static __attribute__((used)) void print_relevant_queues(void) {
    for(int i = 0; i < 4; i++) {
        print_queue(i);
    }
}

static struct thread* queue_get_thread(struct run_queue* queue) {
    struct thread* thread = queue->head;

    while(thread) {
        if(thread->pin < 0 || thread->pin == _cpu()->id)
            break;

        thread = thread->next;
    }

    if(!thread)
        return nullptr;

    if(thread->prev)
        thread->prev->next = thread->next;
    else
        queue->head = thread->next;

    if(thread->next)
        thread->next->prev = thread->prev;
    else
        queue->last = thread->prev;

    return thread;
}

static struct thread* dequeue_next_thread(int min_priority) {
    if(run_queue_bitmap == 0)
        return nullptr;

    bool int_state = interrupt_set(false);

    struct thread* thread = nullptr;

    for(int i = 0; i < RUN_QUEUE_COUNT && i <= min_priority && !thread; i++) {
        thread = queue_get_thread(&run_queue[i]);
        if(thread && !run_queue[i].head)
            run_queue_bitmap &= ~((uint64_t) 1 << i);
    }

    if(thread)
        thread->flags &= ~THREAD_FLAGS_QUEUED;

    interrupt_set(int_state);

#ifdef _SCHED_DEBUG
    if(thread) {
        klog(DEBUG, "[cpu %d] Dequeueing tid %d [pid %d]", _cpu()->id, thread->tid, thread->proc ? thread->proc->pid : -1);
        print_relevant_queues();
    }
#endif

    return thread;
}

static void enqueue_thread(struct thread* thread) {
#ifdef _SCHED_DEBUG
    klog(DEBUG, "[cpu %d] Enqueueing tid %d [pid %d]", _cpu()->id, thread->tid, thread->proc ? thread->proc->pid : -1);
#endif

    if(thread == _cpu()->idle_thread) {
#ifdef _SCHED_DEBUG
        print_relevant_queues();
#endif
        return;
    }

    assert(!(thread->flags & THREAD_FLAGS_RUNNING));
    thread->flags |= THREAD_FLAGS_QUEUED;

    run_queue_bitmap |= ((uint64_t) 1 << thread->priority);

    thread->prev = run_queue[thread->priority].last;
    if(thread->prev)
        thread->prev->next = thread;
    else 
        run_queue[thread->priority].head = thread;

    thread->next = nullptr;
    run_queue[thread->priority].last = thread;

#ifdef _SCHED_DEBUG
    print_relevant_queues();
#endif
}

static __noreturn void switch_thread(struct thread* thread) {
    interrupt_set(false);

    struct thread* current = current_thread();
    _cpu()->thread = thread;
    
    if(!current || thread->vmm_context != current->vmm_context)
        vmm_switch_context(thread->vmm_context);
 
#ifdef _SCHED_DEBUG
    if(current && current->tid != thread->tid)
        klog(DEBUG, "cpu %u: switching from [tid %d / pid %d] to [tid %d / pid %d]", _cpu()->id, current ? current->tid : -1, current && current->proc ? current->proc->pid : -1, thread->tid, thread->proc ? thread->proc->pid : -1);    
#endif

    _cpu()->interrupt_status = CPU_CONTEXT_INTSTATUS(&thread->context);
    thread->cpu = _cpu();

    spinlock_acquire(&run_queue_lock);
    if(current)
        current->flags &= ~THREAD_FLAGS_RUNNING;

    assert(!(thread->flags & THREAD_FLAGS_RUNNING));

    if(current && current->flags & THREAD_FLAGS_SLEEP)
        spinlock_release(&current->sleep_lock);

    thread->flags |= THREAD_FLAGS_RUNNING;
    assert(!(thread->flags & THREAD_FLAGS_QUEUED));

    spinlock_release(&run_queue_lock);

    void* scheduler_stack = _cpu()->scheduler_stack;
    assert(!((void*) CPU_SP(&thread->context) < scheduler_stack && (void*) CPU_SP(&thread->context) >= (scheduler_stack - SCHEDULER_STACK_SIZE)));

    CPU_CONTEXT_SWITCHTHREAD(thread);
    here();
    unreachable();
}

static __noreturn void preempt_callback(void) {
    spinlock_acquire(&run_queue_lock);

    struct thread* current = current_thread();
    struct thread* next = dequeue_next_thread(current->priority);

    current->flags &= ~THREAD_FLAGS_PREEMPTED;
    if(next) {
        current->flags &= ~THREAD_FLAGS_RUNNING;
        enqueue_thread(current);
    }
    else
        next = current;

    spinlock_release(&run_queue_lock);    

    assert(!(next->flags & THREAD_FLAGS_QUEUED));
    switch_thread(next);
}

__noreturn void sched_stop_thread(void) {
    interrupt_set(false);

    spinlock_acquire(&run_queue_lock);
    if(current_thread())
        current_thread()->flags &= ~THREAD_FLAGS_RUNNING;

    struct thread* next = dequeue_next_thread(0x0fffffff);
    if(next) {
        assert(!(next->flags & THREAD_FLAGS_QUEUED));
    }
    else {
#ifdef _SCHED_DEBUG
        klog(WARN, "[cpu %d] idle", _cpu()->id);
#endif
        next = _cpu()->idle_thread;
    }

    spinlock_release(&run_queue_lock);

    switch_thread(next);
}

// cpu timer callback
static void timer_hook(struct cpu_context* context, dpc_arg_t arg __unused) {
    struct thread* current = current_thread();
    interrupt_set(false);

    calc_global_load_tick();    

    // already preempted
    if(current->flags & THREAD_FLAGS_PREEMPTED)
        return;

    current->flags |= THREAD_FLAGS_PREEMPTED;

    CPU_CONTEXT_THREADSAVE(current, context);

    // switch stack back to scheduler
    cpu_ctx_init(context, false, false);
    CPU_SP(context) = (uintptr_t) _cpu()->scheduler_stack;

    // drop down to the scheduler
    CPU_IP(context) = (uintptr_t) preempt_callback;
}

void sched_prepare_sleep(bool interruptible) {
    struct thread* thread = current_thread();

    thread->sleep_interrupt_status = interrupt_set(false);
    spinlock_acquire(&thread->sleep_lock);
    thread->flags |= THREAD_FLAGS_SLEEP | (interruptible ? THREAD_FLAGS_INTERRUPTIBLE : 0);
}

static void timeout(struct cpu_context* ctx __unused, dpc_arg_t arg) {
    sched_wakeup((struct thread*) arg, 0);
}

static void yield(struct cpu_context* context, void* __unused) {
    struct thread* thread = current_thread();

    spinlock_acquire(&run_queue_lock);

    bool sleeping = thread->flags & THREAD_FLAGS_SLEEP;

    struct thread* next = dequeue_next_thread(sleeping ? 0x0fffffff : thread->priority);

    bool got_signal = false;
    // TODO: signal handling
    
    if(sleeping && (thread->should_exit || got_signal) && (thread->flags & THREAD_FLAGS_INTERRUPTIBLE)) {
        sleeping = false;
        if(next)
            enqueue_thread(next);
        next = nullptr;
        thread->flags &= ~(THREAD_FLAGS_SLEEP | THREAD_FLAGS_INTERRUPTIBLE);
        thread->wakeup_reason = WAKEUP_REASON_INTERRUPTED;
        spinlock_release(&thread->sleep_lock);
    }

    if(next || sleeping) {
        CPU_CONTEXT_THREADSAVE(thread, context);

        thread->flags &= ~THREAD_FLAGS_RUNNING;
        if(!sleeping)
            enqueue_thread(thread);

        if(!next)
            next = _cpu()->idle_thread;

        spinlock_release(&run_queue_lock);

        if(next->flags & THREAD_FLAGS_QUEUED) {
            klog(ERROR, "is_idle_thread: %d", _cpu()->idle_thread == next);
        }
        switch_thread(next);
    }

    spinlock_release(&run_queue_lock);
}

void sched_sleep(size_t us) {
    struct timer_entry sleep_entry = {0};
    sched_prepare_sleep(false);

    timer_insert(_cpu()->timer, &sleep_entry, timeout, current_thread(), us, false);
    sched_yield();
}

bool sched_wakeup(struct thread* thread, enum wakeup_reason reason) {
    if(!thread)
        return false;

    bool int_state = interrupt_set(false);

    spinlock_acquire(&thread->sleep_lock);

    if((thread->flags & THREAD_FLAGS_SLEEP) == 0 || (reason == WAKEUP_REASON_INTERRUPTED && (thread->flags & THREAD_FLAGS_INTERRUPTIBLE) == 0)) {
        spinlock_release(&thread->sleep_lock);
        interrupt_set(int_state);
        return false;
    }

    thread->flags &= ~(THREAD_FLAGS_SLEEP | THREAD_FLAGS_INTERRUPTIBLE);
    thread->wakeup_reason = reason;

    int err = sched_queue(thread);
    if(err) {
        assert(!"tried queueing an existing thread");
    }

    spinlock_release(&thread->sleep_lock);
    interrupt_set(int_state);

    return !err;
}

int sched_queue(struct thread* thread) {
    bool int_state = interrupt_set(false);
    spinlock_acquire(&run_queue_lock);

    if(thread->flags & THREAD_FLAGS_QUEUED || thread->flags & THREAD_FLAGS_RUNNING)
        return EEXIST;

    enqueue_thread(thread);

    spinlock_release(&run_queue_lock);
    interrupt_set(int_state);

    // TODO: yield to higher priority threads
    return 0;
}

int sched_yield(void) {
    assert(_cpu()->ipl == IPL_NORMAL);
    
    struct thread* thread = current_thread();
    assert(thread);

    bool sleeping = thread->flags & THREAD_FLAGS_SLEEP;
    bool old = sleeping ? thread->sleep_interrupt_status : interrupt_set(false);

    _context_save_and_call(yield, _cpu()->scheduler_stack, nullptr);
    interrupt_set(old);

    return sleeping ? thread->wakeup_reason : 0;
}

// main scheduler entry
void scheduler_init(void) {
    proc_init();
    thread_init();

    _cpu()->scheduler_stack = vmm_map(nullptr, SCHEDULER_STACK_SIZE, VMM_FLAGS_ALLOCATE, MMU_FLAGS_READ | MMU_FLAGS_WRITE | MMU_FLAGS_NOEXEC, nullptr);
    assert(_cpu()->scheduler_stack);
    _cpu()->scheduler_stack = (void*)((uintptr_t) _cpu()->scheduler_stack + SCHEDULER_STACK_SIZE);

    spinlock_init(run_queue_lock);

    _cpu()->idle_thread = thread_create(cpu_idle_thread, PAGE_SIZE * 4, 3, nullptr, nullptr);
    assert(_cpu()->idle_thread);
    _cpu()->thread = thread_create(nullptr, PAGE_SIZE * 32, 0, nullptr, nullptr);
    assert(current_thread());

    // install scheduling timer
    timer_insert(_cpu()->timer, &_cpu()->sched_timer_entry, timer_hook, nullptr, QUANTUM_US, true);
    
    // start scheduling timer
    timer_resume(_cpu()->timer);
}

// scheduler entry point for all smp cores
void scheduler_apentry(void) {
    _cpu()->scheduler_stack = vmm_map(nullptr, SCHEDULER_STACK_SIZE, VMM_FLAGS_ALLOCATE, MMU_FLAGS_READ | MMU_FLAGS_WRITE | MMU_FLAGS_NOEXEC, nullptr);
    assert(_cpu()->scheduler_stack);
    _cpu()->scheduler_stack = (void*)((uintptr_t) _cpu()->scheduler_stack + SCHEDULER_STACK_SIZE);

    _cpu()->idle_thread = thread_create(cpu_idle_thread, PAGE_SIZE * 4, 3, nullptr, nullptr);
    assert(_cpu()->idle_thread);

    // install scheduling timer
    timer_insert(_cpu()->timer, &_cpu()->sched_timer_entry, timer_hook, nullptr, QUANTUM_US, true);

    // start scheduling timer
    timer_resume(_cpu()->timer);
}

static void _internal_thread_exit(struct cpu_context* __unused, void* __unused) {
    struct thread* thread = current_thread();
    _cpu()->thread = nullptr;

    interrupt_set(false);
    spinlock_acquire(&run_queue_lock);
    thread->flags |= THREAD_FLAGS_DEAD;
    spinlock_release(&run_queue_lock);

    sched_stop_thread();
}

static void sched_proc_exit(void) {
    struct proc* proc = current_proc();
    PROC_HOLD(proc);

    for(size_t fd = 0; fd < proc->fd_count; fd++)
        fd_close((int) fd);

    assert(proc->parent != nullptr);

    mutex_acquire(&proc->mutex, false);
    mutex_acquire(&proc->parent->mutex, false);

    vop_release(&proc->root);
    vop_release(&proc->cwd);

    proc->state = PROC_STATE_ZOMBIE;
    struct proc* last_child = proc->child;

    while(last_child && last_child->sibling) {
        last_child->parent = proc->parent;
        last_child = last_child->sibling;
    }

    if(last_child) {
        last_child->sibling = proc->parent->child;
        proc->parent->child = proc->child;
    }

    // TODO: signals

    mutex_release(&proc->parent->mutex);
    mutex_release(&proc->mutex);

    semaphore_signal(&proc->parent->wait_sem);

    PROC_RELEASE(proc);
}

static __noreturn void sched_thread_exit(void) {
    struct thread* thread = current_thread();
    assert(thread);

    struct proc* proc = thread->proc;

    struct vmm_context* old_ctx = thread->vmm_context;
    vmm_switch_context(&vmm_kernel_context);

    if(proc) {
        __atomic_fetch_sub(&proc->running_thread_count, 1, __ATOMIC_SEQ_CST);
        assert(old_ctx != &vmm_kernel_context);

        if(!proc->running_thread_count) {
            if(thread->should_exit)
                proc->status = -1;

            sched_proc_exit();
            vmm_context_destroy(old_ctx);
            PROC_RELEASE(proc);
        }
    }

    interrupt_set(false);
    _context_save_and_call(_internal_thread_exit, _cpu()->scheduler_stack, nullptr);
    unreachable();
}

void sched_stop_other_threads(void) {
    // TODO:
}

static void userspace_check(struct check_args* __unused) {
    struct thread* thread = current_thread();
    assert(thread);

    if(thread->should_exit) {
        interrupt_set(true);
        sched_thread_exit();
    }

    // TODO: check signals
}

static void userspace_check_routine(struct cpu_context* ctx, void* userp) {
    assert(!CPU_CONTEXT_INTSTATUS(ctx));

    struct check_args* args = userp;
    userspace_check(args);
    _cpu()->interrupt_status = CPU_CONTEXT_INTSTATUS(args->context);

    _context_switch(args->context);
}

// called from `_context_switch()` and `_syscall_entry()`
extern __syscall void _sched_userspace_check(struct cpu_context* context, bool syscall, uint64_t syscall_errno, uint64_t syscall_ret) {
    if(!current_thread() || !cpu_ctx_is_user(context))
        return;

//    klog(ERROR, "in _sched_userspace_check()");
    bool int_status = interrupt_set(false);

    struct check_args args = {
        .context = context,
        .syscall = syscall,
        .syscall_errno = syscall_errno,
        .syscall_ret = syscall_ret
    };

    // check if we are on the scheduler/kernel stack
    if((void*) &args < _cpu()->scheduler_stack && (void*) &args >= _cpu()->scheduler_stack - SCHEDULER_STACK_SIZE)
        _context_save_and_call(userspace_check_routine, current_thread()->kernel_stack_top, &args); // switch to kernel stack first
    else
        userspace_check(&args);

    _cpu()->interrupt_status = int_status;
}

static int load_interpreter(const char* interpreter, void** entry) {
    if(!interpreter)
        return 0;

    int err;
    struct vnode* interp_node;

    if((err = vfs_open(vfs_root, interpreter, 0, &interp_node)))
        return err;

    Elf64_auxv_list_t interp_auxv;
    char* interp_interp = nullptr;
    void* interp_brk;

    if((err = elf_load(interp_node, INTERPRETER_BASE, entry, &interp_interp, &interp_auxv, &interp_brk)))
        return err;

    assert(!interp_interp);
    return 0;
}

int scheduler_exec(const char* path, char* argv[], char* envp[]) {
    klog(INFO, "executing `%s`", path);

    struct vmm_context* vmm_ctx = vmm_context_new();
    if(!vmm_ctx)
        return ENOMEM;

    vmm_switch_context(vmm_ctx);

    struct proc* proc = proc_create();
    assert(proc);
    
    struct vnode* exec_node;
    int err = vfs_open(vfs_root, path, 0, &exec_node);
    if(err)
        return err;

    Elf64_auxv_list_t auxv;
    char* interpreter = nullptr;
    void* entry;
    void* brk = nullptr;

    if((err = elf_load(exec_node, nullptr, &entry, &interpreter, &auxv, &brk)))
        return err;

    if((err = load_interpreter(interpreter, &entry)))
        return err;

    if(brk)
        klog(DEBUG, "break: %p", brk);

    struct vnode* tty_node;
    assert(devfs_find("tty0", &tty_node) == 0);
    vop_lock(tty_node);
    assert(vop_open(&tty_node, V_FFLAGS_READ | V_FFLAGS_NOCTTY, &proc->cred) == 0);
    vop_hold(tty_node);
    assert(vop_open(&tty_node, V_FFLAGS_WRITE | V_FFLAGS_NOCTTY, &proc->cred) == 0);
    vop_hold(tty_node);
    assert(vop_open(&tty_node, V_FFLAGS_WRITE | V_FFLAGS_NOCTTY, &proc->cred) == 0);
    vop_unlock(tty_node);

    struct file* stdin  = fd_allocate();
    struct file* stdout = fd_allocate();
    struct file* stderr = fd_allocate();

    stdin->vnode = stdout->vnode = stderr->vnode = tty_node;
    stdin->mode = stdout->mode = stderr->mode = 0644;

    stdin->flags = FILE_READ;
    stdout->flags = stderr->flags = FILE_WRITE;

    proc->fd[0] = (struct fd){.file = stdin,  .flags = 0};
    proc->fd[1] = (struct fd){.file = stdout, .flags = 0};
    proc->fd[2] = (struct fd){.file = stderr, .flags = 0};

    proc->cwd = vfs_root;
    vop_hold(vfs_root);
    proc->root = vfs_root;
    vop_hold(vfs_root);
    
    void* stack = elf_prepare_stack(STACK_TOP, &auxv, argv, envp);
    if(!stack)
        return ENOMEM;

    vmm_switch_context(&vmm_kernel_context);

    struct thread* user_thread = thread_create(entry, PAGE_SIZE * 16, 1, proc, stack);
    assert(user_thread);

    vmm_ctx->brk = (struct brk){
        .base = brk,
        .top = brk
    };

    // TODO: proc

    user_thread->vmm_context = vmm_ctx;
    err = sched_queue(user_thread);
    assert(err == 0 && "tried queueing already existing thread");

    vop_release(&exec_node);

    PROC_RELEASE(proc);

    return 0;
}

void scheduler_terminate(int status) {
    struct thread* thread = current_thread();
    assert(thread);

    struct proc* proc = thread->proc;
    if(!spinlock_try(&proc->exiting))
        sched_thread_exit();

    sched_stop_other_threads();
    proc->status = status;

    if(proc->pid == 1)
        panic("`init` process (pid 1) terminated. This should never happen!");

    sched_thread_exit();
}

