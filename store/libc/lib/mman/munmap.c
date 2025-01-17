#include <sys/mman.h>

#include <sys/syscall.h>
#include <unistd.h>

int munmap(void* addr, size_t len) {
    return syscall(SYS_munmap, addr, len);
}
