#include <unistd.h>
#include <stdlib.h>

#include <sys/syscall.h>
#include <internal/syscall.h>

int access(const char *path, int amode) {
    return syscall(SYS_access, path, amode);
}

int open(const char *pathname, int flags, mode_t mode) {
    return syscall(SYS_open, pathname, flags, mode);
}

int close(int fd) {
    return syscall(SYS_close, fd);
}

ssize_t read(int fd, void* buf, size_t count) {
    return syscall(SYS_read, fd, buf, count);
}

ssize_t write(int fd, const void* buf, size_t size) {
    return syscall(SYS_write, fd, buf, size);
}

_Noreturn void _exit(int status) {
    _Exit(status);
}

int fork(void) {
    return syscall(SYS_fork);
}

int uname(struct utsname *utsname) {
    return syscall(SYS_uname, utsname);
}

pid_t getpid(void) {
    return syscall(SYS_getpid);
}
