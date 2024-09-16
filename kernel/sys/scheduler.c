#include "mem/heap.h"
#include "sys/fd.h"
#include "sys/semaphore.h"
#include "sys/syscall.h"
#include "x86_64/cpu/cpu.h"
#include "x86_64/cpu/idt.h"
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

#define SCHEDULER_STACK_SIZE (PAGE_SIZE * 16)

// main timer quantum
#define QUANTUM_US 10'000

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

static struct scache* thread_cache;
static struct scache* proc_cache;

static spinlock_t run_queue_lock;
static uint64_t run_queue_bitmap;

static struct run_queue run_queue[RUN_QUEUE_COUNT];

static hashtable_t pid_table;
mutex_t sched_pid_table_mutex;

static pid_t current_pid = 1;

void sched_pin(cpuid_t pin) {
    bool interrupt_status = interrupt_set(false);
    _cpu()->thread->pin = pin;
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
        run_queue->last = thread->prev;

    return thread;
}

static struct thread* dequeue_next_thread(int min_priority) {
    if(run_queue_bitmap == 0)
        return nullptr;

    bool int_state = interrupt_set(false);

    struct thread* thread = nullptr;

    for(int i = 0; i < RUN_QUEUE_COUNT && i <= min_priority && !thread; i++) {
        if(!(run_queue_bitmap & ((uint64_t) 1 << i)))
            continue;

        thread = queue_get_thread(&run_queue[i]);
        if(thread && !run_queue[i].head)
            run_queue_bitmap &= ~((uint64_t) 1 << i);
    }

    if(thread)
        thread->flags &= ~THREAD_FLAGS_QUEUED;

    interrupt_set(int_state);
    return thread;
}

static void enqueue_thread(struct thread* thread) {
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
}

static __noreturn void switch_thread(struct thread* thread) {
    interrupt_set(false);

    struct thread* current = _cpu()->thread;
    _cpu()->thread = thread;

    if(!current || thread->vmm_context != current->vmm_context)
        vmm_switch_context(thread->vmm_context);

    _cpu()->interrupt_status = CPU_CONTEXT_INTSTATUS(&thread->context);
    thread->cpu = _cpu();

    spinlock_acquire(&run_queue_lock);
    if(current)
        current->flags &= ~THREAD_FLAGS_RUNNING;

    assert(!(thread->flags & THREAD_FLAGS_RUNNING));

    if(current && current->flags & THREAD_FLAGS_SLEEP)
        spinlock_release(&current->sleep_lock);

    thread->flags |= THREAD_FLAGS_RUNNING;
//    assert(!(thread->flags & THREAD_FLAGS_QUEUED));

    spinlock_release(&run_queue_lock);

    void* scheduler_stack = _cpu()->scheduler_stack;
    assert(!((void*) CPU_SP(&thread->context) < scheduler_stack && (void*) CPU_SP(&thread->context) >= (scheduler_stack - SCHEDULER_STACK_SIZE)));

    CPU_CONTEXT_SWITCHTHREAD(thread);
    here();
    unreachable();
}

static __noreturn void preempt_callback(void) {
    spinlock_acquire(&run_queue_lock);

    struct thread* current = _cpu()->thread;
    struct thread* next = dequeue_next_thread(current->priority);

    current->flags &= ~THREAD_FLAGS_PREEMPTED;
    if(next) {
        current->flags &= ~THREAD_FLAGS_RUNNING;
        enqueue_thread(current);
    }
    else
        next = current;

    spinlock_release(&run_queue_lock);    

    switch_thread(next);
}

__noreturn void sched_stop_thread(void) {
    interrupt_set(false);

    spinlock_acquire(&run_queue_lock);
    if(_cpu()->thread)
        _cpu()->thread->flags &= ~THREAD_FLAGS_RUNNING;

    struct thread* next = dequeue_next_thread(0x0fffffff);
    if(!next)
        next = _cpu()->idle_thread;

    spinlock_release(&run_queue_lock);

    switch_thread(next);
}

// cpu timer callback
static void timer_hook(struct cpu_context* context, dpc_arg_t arg __unused) {
    struct thread* current = _cpu()->thread;
    interrupt_set(false);

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
    _cpu()->thread->sleep_interrupt_status = interrupt_set(false);
    spinlock_acquire(&_cpu()->thread->sleep_lock);
    _cpu()->thread->flags |= THREAD_FLAGS_SLEEP | (interruptible ? THREAD_FLAGS_INTERRUPTIBLE : 0);
}

static void timeout(struct cpu_context* ctx __unused, dpc_arg_t arg) {
    sched_wakeup((struct thread*) arg, 0);
}

static void _yield(struct cpu_context* context, void* __unused) {
    struct thread* thread = _cpu()->thread;

    spinlock_acquire(&run_queue_lock);

    bool sleeping = thread->flags & THREAD_FLAGS_SLEEP;

    struct thread* next = dequeue_next_thread(sleeping ? 0x0fffffff : thread->priority);

    bool got_signal = false;
    // TODO: signal handling
    
    if(sleeping && (thread->should_exit || got_signal) && (thread->flags & THREAD_FLAGS_INTERRUPTIBLE)) {
        sleeping = false;
        next = nullptr;
        thread->flags &= ~(THREAD_FLAGS_SLEEP | THREAD_FLAGS_INTERRUPTIBLE);
        thread->wakeup_reason = WAKEUP_REASON_INTERRUPTED;
        spinlock_release(&thread->sleep_lock);
    }

    if(next || sleeping) {
        CPU_CONTEXT_THREADSAVE(thread, context);

        thread->flags &= ~THREAD_FLAGS_RUNNING;
        if(sleeping == false)
            enqueue_thread(thread);

        if(!next)
            next = _cpu()->idle_thread;

        spinlock_release(&run_queue_lock);
        switch_thread(next);
    }

    spinlock_release(&run_queue_lock);
}

void sched_sleep(size_t us) {
    struct timer_entry sleep_entry = {0};
    sched_prepare_sleep(false);

    timer_insert(_cpu()->timer, &sleep_entry, timeout, _cpu()->thread, us, false);
    sched_yield();
}

bool sched_wakeup(struct thread* thread, enum wakeup_reason reason) {
    bool int_state = interrupt_set(false);

    spinlock_acquire(&thread->sleep_lock);

    if((thread->flags & THREAD_FLAGS_SLEEP) == 0 || (reason == WAKEUP_REASON_INTERRUPTED && (thread->flags & THREAD_FLAGS_INTERRUPTIBLE) == 0)) {
        spinlock_release(&thread->sleep_lock);
        interrupt_set(int_state);
        return false;
    }

    thread->flags &= ~(THREAD_FLAGS_SLEEP | THREAD_FLAGS_INTERRUPTIBLE);
    thread->wakeup_reason = reason;

    sched_queue(thread);
    spinlock_release(&thread->sleep_lock);
    interrupt_set(int_state);

    return true;
}

void sched_queue(struct thread* thread) {
    bool int_state = interrupt_set(false);
    spinlock_acquire(&run_queue_lock);

    assert((thread->flags & THREAD_FLAGS_QUEUED) == 0 && (thread->flags & THREAD_FLAGS_RUNNING) == 0);

    enqueue_thread(thread);

    spinlock_release(&run_queue_lock);
    interrupt_set(int_state);

    // TODO: yield to higher priority threads
}

int sched_yield(void) {
    assert(_cpu()->ipl == IPL_NORMAL);
    
    bool sleeping = _cpu()->thread->flags & THREAD_FLAGS_SLEEP;
    bool old = sleeping ? _cpu()->thread->sleep_interrupt_status : interrupt_set(false);

    _context_save_and_call(_yield, _cpu()->scheduler_stack, nullptr);
    interrupt_set(old);

    return sleeping ? _cpu()->thread->wakeup_reason : 0;
}

// main scheduler entry
void scheduler_init(void) {
    thread_cache = slab_newcache(sizeof(struct thread), 0, nullptr, nullptr);
    assert(thread_cache);
    proc_cache = slab_newcache(sizeof(struct proc), 0, nullptr, nullptr);
    assert(proc_cache);

    assert(hashtable_init(&pid_table, 100) == 0);
    mutex_init(&sched_pid_table_mutex);

    _cpu()->scheduler_stack = vmm_map(nullptr, SCHEDULER_STACK_SIZE, VMM_FLAGS_ALLOCATE, MMU_FLAGS_READ | MMU_FLAGS_WRITE | MMU_FLAGS_NOEXEC, nullptr);
    assert(_cpu()->scheduler_stack);
    _cpu()->scheduler_stack = (void*)((uintptr_t) _cpu()->scheduler_stack + SCHEDULER_STACK_SIZE);

    spinlock_init(run_queue_lock);

    _cpu()->idle_thread = sched_new_thread(cpu_idle_thread, PAGE_SIZE * 4, 3, nullptr, nullptr);
    assert(_cpu()->idle_thread);
    _cpu()->thread = sched_new_thread(nullptr, PAGE_SIZE * 32, 0, nullptr, nullptr);
    assert(_cpu()->thread);

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

    _cpu()->idle_thread = sched_new_thread(cpu_idle_thread, PAGE_SIZE * 4, 3, nullptr, nullptr);
    assert(_cpu()->idle_thread);

    // install scheduling timer
    timer_insert(_cpu()->timer, &_cpu()->sched_timer_entry, timer_hook, nullptr, QUANTUM_US, true);

    // start scheduling timer
    timer_resume(_cpu()->timer);
}

struct thread* sched_new_thread(void* ip, size_t kernel_stack_size, int priority, struct proc* proc, void* user_stack) {
    struct thread* thread = slab_alloc(thread_cache);
    if(!thread)
        return nullptr;

    memset(thread, 0, sizeof(struct thread));

    thread->kernel_stack = vmm_map(nullptr, kernel_stack_size, VMM_FLAGS_ALLOCATE, MMU_FLAGS_WRITE | MMU_FLAGS_READ | MMU_FLAGS_NOEXEC, nullptr);
    if(!thread->kernel_stack) {
        slab_free(thread_cache, thread);
        return nullptr;
    }

    thread->kernel_stack_top = (void*)((uintptr_t) thread->kernel_stack + kernel_stack_size);
    thread->pin = THREAD_UNPINNED;
    
    thread->vmm_context = proc ? nullptr : &vmm_kernel_context;
    thread->proc = proc;
    thread->priority = priority;
    if(proc) {
        PROC_HOLD(proc);
        thread->tid = __atomic_fetch_add(&current_pid, 1, __ATOMIC_SEQ_CST);
    }

    cpu_ctx_init(&thread->context, proc, true);
    cpu_extra_ctx_init(&thread->extra_context);

    CPU_SP(&thread->context) = proc ? (register_t) user_stack : (register_t) thread->kernel_stack_top;
    CPU_IP(&thread->context) = (register_t) ip;

    spinlock_init(thread->sleep_lock);
    spinlock_init(thread->signals.lock);

    return thread;
}

struct proc* sched_new_proc(void) {
    struct proc* proc = slab_alloc(proc_cache);
    if(!proc)
        return nullptr;

    memset(proc, 0, sizeof(struct proc));

    mutex_init(&proc->mutex);
    spinlock_init(proc->nodes_lock);
    
    proc->pid = __atomic_fetch_add(&current_pid, 1, __ATOMIC_SEQ_CST);

    proc->ref_count = 1;
    proc->state = PROC_STATE_NORMAL;
    proc->running_thread_count = 1;
    
    proc->fd_count = 3;
    proc->fd_first = 1;
    mutex_init(&proc->fd_mutex);

    proc->fd = kmalloc(sizeof(struct fd) * proc->fd_count);
    if(!proc->fd) {
        slab_free(proc_cache, proc);
        return nullptr;
    }

    proc->umask = 022;
    
    semaphore_init(&proc->wait_sem, 0);

    mutex_acquire(&sched_pid_table_mutex, false);
    if(hashtable_set(&pid_table, proc, &proc->pid, sizeof(pid_t), true)) {
        mutex_release(&sched_pid_table_mutex);
        kfree(proc->fd);
        slab_free(proc_cache, proc);
        return nullptr;
    }
    mutex_release(&sched_pid_table_mutex);

    return proc;
}

static void userspace_check(struct check_args* args) {
    if(_cpu()->thread->should_exit) {
        interrupt_set(true);
        // THREAD EXIT
        unimplemented();
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
    assert(_cpu());
    if(!_cpu()->thread || !cpu_ctx_is_user(context))
        return;

//    klog(ERROR, "in _sched_userspace_check()");
    bool int_status = interrupt_set(false);

    struct check_args args = {
        .context = context,
        .syscall = syscall,
        .syscall_errno = syscall_errno,
        .syscall_ret = syscall_ret
    };

    // check if we are on the scheduler stack
    if((void*) &args < _cpu()->scheduler_stack && (void*) &args >= _cpu()->scheduler_stack - SCHEDULER_STACK_SIZE)
        //unimplemented(); // switch stacks
        _context_save_and_call(userspace_check_routine, _cpu()->thread->kernel_stack_top, &args);
    else
        userspace_check(&args);

    _cpu()->interrupt_status = int_status;
}

#define STACK_TOP        ((void*) 0x0000800000000000)
#define INTERPRETER_BASE ((void*) 0x00000beef0000000)

static int load_interpreter(const char* interpreter, void** entry) {
    if(!interpreter)
        return 0;

    int err;
    struct vnode* interp_node;

    if((err = vfs_open(vfs_root, interpreter, 0, &interp_node)))
        return err;

    Elf64_auxv_list_t interp_auxv;
    char* interp_interp = nullptr;

    if((err = elf_load(interp_node, INTERPRETER_BASE, entry, &interp_interp, &interp_auxv)))
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

    struct proc* proc = sched_new_proc();
    assert(proc);
    
    struct vnode* exec_node;
    int err = vfs_open(vfs_root, path, 0, &exec_node);
    if(err)
        return err;

    Elf64_auxv_list_t auxv;
    char* interpreter = nullptr;
    void* entry;

    if((err = elf_load(exec_node, nullptr, &entry, &interpreter, &auxv)))
        return err;

    if((err = load_interpreter(interpreter, &entry)))
        return err;

    // TODO: fd allocation

    void* stack = elf_prepare_stack(STACK_TOP, &auxv, argv, envp);
    if(!stack)
        return ENOMEM;

    vmm_switch_context(&vmm_kernel_context);

    struct thread* user_thread = sched_new_thread(entry, PAGE_SIZE * 16, 1, proc, stack);
    assert(user_thread);

    // TODO: proc

    user_thread->vmm_context = vmm_ctx;
    sched_queue(user_thread);

    vop_release(&exec_node);

    // proc_release(&proc);

    return 0;
}

