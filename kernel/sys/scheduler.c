#include "x86_64/mem/mmu.h"
#include <sys/scheduler.h>
#include <sys/thread.h>
#include <sys/mutex.h>
#include <sys/spinlock.h>

#include <cpu/cpu.h>
#include <mem/slab.h>
#include <mem/vmm.h>

#include <hashtable.h>
#include <assert.h>
#include <string.h>
#include <abi.h>

#define SCHEDULER_STACK_SIZE (PAGE_SIZE * 16)

static struct scache* thread_cache;
static struct scache* proc_cache;

static spinlock_t run_queue_lock;

static hashtable_t pid_table;
mutex_t sched_pid_table_mutex;

static pid_t current_pid = 1;

static void cpu_idle_thread(void) {}

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

    // TODO: timer init
}

void scheduler_apentry(void) {
    _cpu()->scheduler_stack = vmm_map(nullptr, SCHEDULER_STACK_SIZE, VMM_FLAGS_ALLOCATE, MMU_FLAGS_READ | MMU_FLAGS_WRITE | MMU_FLAGS_NOEXEC, nullptr);
    assert(_cpu()->scheduler_stack);
    _cpu()->scheduler_stack = (void*)((uintptr_t) _cpu()->scheduler_stack + SCHEDULER_STACK_SIZE);

    _cpu()->idle_thread = sched_new_thread(cpu_idle_thread, PAGE_SIZE * 4, 3, nullptr, nullptr);
    assert(_cpu()->idle_thread);
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
