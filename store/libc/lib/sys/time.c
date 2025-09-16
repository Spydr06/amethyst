#include <sys/time.h>

#include <sys/syscall.h>
#include <internal/syscall.h>

int gettimeofday(struct timeval *tv, void *restrict __unused) {
    return syscall(SYS_gettimeofday, tv);
}

