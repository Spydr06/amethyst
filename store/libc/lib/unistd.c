#include <unistd.h>
#include <stdlib.h>

#include <sys/syscall.h>
#include <internal/syscall.h>

ssize_t write(int fd, const void* buf, size_t size) {
    return syscall(SYS_write, fd, buf, size);
}

_Noreturn void _exit(int status) {
    _Exit(status);
}

