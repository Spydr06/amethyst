#include <sys/stat.h>

#include <sys/syscall.h>
#include <unistd.h>

int mkdir(const char *pathname, mode_t mode) {
    return syscall(SYS_mkdir, pathname, mode);
}

int fstat(int fd, struct stat *restrict buf) {
    return syscall(SYS_fstat, fd, buf);
}

int stat(const char *restrict pathname, struct stat *restrict buf) {
    return syscall(SYS_stat, pathname, buf);
}

