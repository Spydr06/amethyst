#include <unistd.h>
#include <stdarg.h>
#include <stdlib.h>

#include <sys/syscall.h>
#include <internal/syscall.h>

int access(const char *path, int amode) {
    return syscall(SYS_access, path, amode);
}

int dup(int oldfd) {
    return syscall(SYS_dup, oldfd);
}

int dup2(int oldfd, int newfd) {
    return syscall(SYS_dup2, oldfd, newfd);
}

int open(const char *pathname, int flags, ...) {
    va_list ap;
    va_start(ap, flags);

    mode_t mode = va_arg(ap, mode_t);
    int res = syscall(SYS_open, pathname, flags, mode);

    va_end(ap);
    return res;
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

off_t lseek(int fd, off_t offset, int whence) {
    return syscall(SYS_lseek, fd, offset, whence);
}

_Noreturn void _exit(int status) {
    _Exit(status);
}

int fork(void) {
    return syscall(SYS_fork);
}

int vfork(void) {
    // FIXME!
    return fork();
}

int pipe2(int pipefd[2], int flags) {
    return syscall(SYS_pipe, pipefd, flags);
}

int pipe(int pipefd[2]) {
    return pipe2(pipefd, 0);
}

int uname(struct utsname *utsname) {
    return syscall(SYS_uname, utsname);
}

pid_t getpid(void) {
    return syscall(SYS_getpid);
}

uid_t getuid(void) {

}

uid_t geteuid(void) {

}

gid_t getgid(void) {

}

gid_t getegid(void) {

}

int chdir(const char* pathname) {
    return syscall(SYS_chdir, pathname);
}

int fchdir(int fd) {
    return syscall(SYS_fchdir, fd);
}

