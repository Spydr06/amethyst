#include <sys/syscall.h>
#include <sys/mmap.h>
#include <sys/fd.h>
#include <mem/vmm.h>

#include <math.h>
#include <errno.h>

__syscall syscallret_t _sys_mmap(struct cpu_context* __unused, void* addr, size_t len, enum map_prot prot, enum map_flags flags, int fd, off_t offset) {
    syscallret_t ret = {
        ._errno = 0,
        .ret = -1
    };

    if((flags & MAP_PRIVATE) && (flags & MAP_SHARED)) {
        ret._errno = EINVAL;
        return ret;
    }

    if(len == 0 || (uintptr_t) addr % PAGE_SIZE) {
        ret._errno = EINVAL;
        return ret;
    }

    bool as_file = (flags & MAP_ANONYMOUS) == 0;

    enum mmu_flags mmu_flags = MMU_FLAGS_USER | prot_to_mmu_flags(prot);
    enum vmm_flags vmm_flags = mmap_to_vmm_flags(flags);

    struct vmm_file_desc vfd;
    struct file* file = nullptr;

    if(as_file) {
        if(offset & PAGE_SIZE) {
            ret._errno = EINVAL;
            return ret;
        }

        if(!(file = fd_get(fd))) {
            ret._errno = EBADF;
            return ret;
        }
        
        vfd.node = file->vnode;
        vfd.offset = offset;
    }
    else
        vmm_flags |= VMM_FLAGS_ALLOCATE;

    addr = MAX(MIN(addr, USERSPACE_END), USERSPACE_START);

    ret.ret = (uint64_t) vmm_map(addr, len, vmm_flags, mmu_flags, as_file ? &vfd : nullptr);
    if(!ret.ret)
        ret._errno = ENOMEM;

    if(as_file)
        fd_release(file);

    return ret;
}

