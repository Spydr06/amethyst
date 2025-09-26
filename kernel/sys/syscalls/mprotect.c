#include <sys/syscall.h>
#include <sys/mmap.h>
#include <mem/vmm.h>

#include <errno.h>

__syscall syscallret_t _sys_mprotect(struct cpu_context* __unused, void* addr, size_t len, enum map_prot prot) {
    syscallret_t ret = {
        ._errno = 0,
        .ret = -1
    };

    if(addr > USERSPACE_END || (uintptr_t) addr % PAGE_SIZE || !len) {
        ret._errno = EINVAL;
        return ret;
    }

    enum mmu_flags mmu_flags = MMU_FLAGS_USER | prot_to_mmu_flags(prot);

    ret._errno = vmm_change_mmu_flags(addr, len, mmu_flags, VMM_FLAGS_CREDCHECK);
    ret.ret = ret._errno ? -1 : 0;

    return ret;
}

