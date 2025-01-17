#include <sys/mman.h>

#include <sys/syscall.h>
#include <internal/syscall.h>

void *mmap(void* addr, size_t len, int prot, int flags, int fd, off_t offset) {
    // TODO: safety checks

    long ret = __syscall(SYS_mmap, addr, len, prot, flags, fd, offset);
    return (void*) __syscall_ret(ret);
}

