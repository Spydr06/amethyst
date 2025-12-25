#include <sys/syscall.h>

#include <sys/thread.h>
#include <mem/vmm.h>

#include <math.h>
#include <stdint.h>
#include <errno.h>

#define BRK_MMU_FLAGS (MMU_FLAGS_READ | MMU_FLAGS_WRITE | MMU_FLAGS_NOEXEC | MMU_FLAGS_USER)

__syscall syscallret_t _sys_brk(struct cpu_context* __unused, void* addr) {
    syscallret_t ret = {
        .ret = -1,
        ._errno = 0,
    };

    struct thread* thread = current_thread();
    if(!thread->vmm_context->brk.base) {
        ret._errno = ENOMEM;
        return ret;
    }

    uintptr_t old_break = (uintptr_t) thread->vmm_context->brk.top; 

    if(!addr) {
        ret.ret = (uint64_t) old_break;
        return ret;
    }
    
    if(addr >= USERSPACE_END || addr < thread->vmm_context->brk.base) {
        ret._errno = EINVAL;
        return ret;
    }
    
    ptrdiff_t diff = (intptr_t) addr - old_break;
    uintptr_t new_break = ROUND_UP((uintptr_t) addr, PAGE_SIZE);

    if(diff > 0) {
        size_t alloc_size = (new_break - old_break) / PAGE_SIZE;

        if(!vmm_map((void*) old_break, alloc_size, VMM_FLAGS_EXACT | VMM_FLAGS_ALLOCATE | VMM_FLAGS_PAGESIZE, BRK_MMU_FLAGS, nullptr)) {
            ret._errno = ENOMEM;
            return ret;
        }
    }
    else if(diff < 0) {
        size_t unmap_size = (old_break - new_break) / PAGE_SIZE;
        vmm_unmap((void*) new_break, unmap_size, VMM_FLAGS_PAGESIZE);
    }
    
    ret.ret = old_break;
    thread->vmm_context->brk.top = (void*) new_break;

    ret._errno = 0;
    return ret;
}

_SYSCALL_REGISTER(SYS_brk, _sys_brk, "brk", "%p");
