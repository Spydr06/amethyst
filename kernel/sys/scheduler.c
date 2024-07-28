#include <sys/scheduler.h>
#include <sys/thread.h>
#include <sys/mutex.h>
#include <sys/spinlock.h>
#include <sys/dpc.h>

#include <cpu/cpu.h>
#include <mem/slab.h>
#include <mem/vmm.h>

#include <hashtable.h>
#include <kernelio.h>
#include <assert.h>
#include <string.h>
#include <abi.h>

#define SCHEDULER_STACK_SIZE (PAGE_SIZE * 16)

// main timer quantum
#define QUANTUM_US 100'000

#define RUN_QUEUE_COUNT 64

struct run_queue {
    struct thread* list;
    struct thread* last;
};

static struct scache* thread_cache;
static struct scache* proc_cache;

static spinlock_t run_queue_lock;
static uint64_t run_queue_bitmap;

static struct run_queue run_queue[RUN_QUEUE_COUNT];

static hashtable_t pid_table;
mutex_t sched_pid_table_mutex;

static pid_t current_pid = 1;

static void sched_target_cpu(struct cpu* cpu) {
    bool interrupt_status = interrupt_set(false);
    _cpu()->thread->cpu_target = cpu;
    interrupt_set(interrupt_status);
}

static void cpu_idle_thread(void) {
//    klog(INFO, "idle (cpu #%d)", _cpu()->id);
    sched_target_cpu(_cpu());

    interrupt_set(true);
    while(1) {
        hlt_until_int();
        sched_yield();
    }
}

static struct thread* queue_get_thread(struct run_queue* queue) {
    struct thread* thread = queue->list;

    while(thread) {
        if(!thread->cpu_target || thread->cpu_target == _cpu())
            break;

        thread = thread->next;
    }

    if(!thread)
        return nullptr;

    if(thread->prev)
        thread->prev->next = thread->next;
    else
        queue->list = thread->next;

    if(thread->next)
        thread->next->prev = thread->prev;
    else
        run_queue->last = thread->prev;

    return thread;
}

static struct thread* dequeue_next_thread(int min_priority) {
    bool int_state = interrupt_set(false);

    struct thread* thread = nullptr;

    if(run_queue_bitmap == 0)
        goto finish;

    for(int i = 0; i < RUN_QUEUE_COUNT && i < min_priority && !thread; i++) {
        thread = queue_get_thread(&run_queue[i]);
        if(thread && !run_queue[i].list)
            run_queue_bitmap &= ~((uint64_t) 1 << i);
    }

finish:
    interrupt_set(int_state);
    return NULL;
}

static void enqueue_thread(struct thread* thread) {
    assert((thread->flags & THREAD_FLAGS_RUNNING) == 0);

    thread->flags |= THREAD_FLAGS_QUEUED;

    run_queue_bitmap |= ((uint64_t) 1 << thread->priority);

    thread->prev = run_queue[thread->priority].last;
    if(thread->prev)
        thread->prev->next = thread;
    else
        run_queue[thread->priority].list = thread;

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

    assert((thread->flags & THREAD_FLAGS_RUNNING) == 0);

    if(current && current->flags & THREAD_FLAGS_SLEEP)
        spinlock_release(&current->sleep_lock);

    thread->flags |= THREAD_FLAGS_RUNNING;
    assert((thread->flags & THREAD_FLAGS_QUEUED) == 0);

    spinlock_release(&run_queue_lock);

    void* scheduler_stack = _cpu()->scheduler_stack;
    assert(!((void*) CPU_SP(&thread->context) < scheduler_stack && (void*) CPU_SP(&thread->context) >= (scheduler_stack - SCHEDULER_STACK_SIZE)));

    CPU_CONTEXT_SWITCHTHREAD(thread);
    here();
    unreachable();
}

static __noreturn void preempt_callback(void) {
    klog(INFO, "in scheduler::preempt_callback() (cpu #%d)", _cpu()->id);

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

static void prepare_sleep(bool interruptible) {
    _cpu()->thread->sleep_interrupt_status = interrupt_set(false);
    spinlock_acquire(&_cpu()->thread->sleep_lock);
    _cpu()->thread->flags |= THREAD_FLAGS_SLEEP | (interruptible ? THREAD_FLAGS_INTERRUPTIBLE : 0);
}

static void timeout(struct cpu_context* ctx __unused, dpc_arg_t arg) {
    sched_wakeup((struct thread*) arg, 0);
}

static void _yield(struct cpu_context* context) {
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
    prepare_sleep(false);

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

    _context_save_and_call(_yield, _cpu()->scheduler_stack);
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

    thread->vmm_context = proc ? nullptr : &vmm_kernel_context;
    thread->proc = proc;
    thread->priority = priority;
    if(proc) {
        PROC_HOLD(proc);
        thread->tid = __atomic_fetch_add(&current_pid, 1, __ATOMIC_SEQ_CST);
    }

    cpu_ctx_init(&thread->context, proc != nullptr, true);
    cpu_extra_ctx_init(&thread->extra_context);

    CPU_SP(&thread->context) = proc ? (register_t) user_stack : (register_t) thread->kernel_stack_top;
    CPU_IP(&thread->context) = (register_t) ip;

    spinlock_init(thread->sleep_lock);
    spinlock_init(thread->signals.lock);

    return thread;
}

// called from `_context_switch()`
extern void _sched_userspace_check(void) {
}

