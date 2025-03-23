#include <sys/thread.h>
#include <sys/proc.h>

#include <mem/slab.h>

#include <assert.h>
#include <string.h>

static struct scache* thread_cache;


void thread_init(void) {
    thread_cache = slab_newcache(sizeof(struct thread), 0, nullptr, nullptr);
    assert(thread_cache);
}

struct thread* thread_create(void* ip, size_t kernel_stack_size, int priority, struct proc* proc, void* user_stack) {
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
        thread->tid = proc_new_pid();
    }

    cpu_ctx_init(&thread->context, proc, true);
    cpu_extra_ctx_init(&thread->extra_context);

    CPU_SP(&thread->context) = proc ? (register_t) user_stack : (register_t) thread->kernel_stack_top;
    CPU_IP(&thread->context) = (register_t) ip;

    spinlock_init(thread->sleep_lock);
    spinlock_init(thread->signals.lock);

    return thread;
}

