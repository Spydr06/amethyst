#include <time.h>

#include <sys/syscall.h>
#include <internal/syscall.h>

int nanosleep(const struct timespec *rqtp, struct timespec *rmtp) {
    return syscall(SYS_nanosleep, rqtp, rmtp);
}
