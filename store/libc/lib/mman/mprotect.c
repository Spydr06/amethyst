#include <sys/mman.h>

#include <sys/syscall.h>
#include <internal/syscall.h>

int mprotect(void *addr, size_t len, int prot) {
    // TODO: safety checks

    long ret = __syscall(SYS_mprotect, addr, len, prot);
    return __syscall_ret(ret);
}

