#include <sys/ioctl.h>

#include <sys/syscall.h>
#include <unistd.h>

int ioctl(int fd, unsigned long request, void *arg) {
    return syscall(SYS_ioctl, fd, request, arg);
}

