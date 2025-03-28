#ifndef _UNISTD_H
#define _UNISTD_H

#include <bits/alltypes.h>

#ifdef __cplusplus
extern "C" {
#endif

#define O_RDONLY    00
#define O_WRONLY    01
#define O_RDWR      02
#define O_CREAT     0100
#define O_EXCL      0200
#define O_NOCTTY    0400
#define O_TRUNC     01000
#define O_APPEND    02000
#define O_NONBLOCK  04000
#define O_DIRECTORY 0100000
#define O_CLOEXEC   02000000

extern char **environ;

int brk(void* addr);
void* sbrk(intptr_t increment);

int execve(const char *pathname, char *const argv[], char *const envp[]);

int execl(const char *pathname, const char *arg, ...);
int execlp(const char *file, const char *arg, ...);
int execle(const char *pathname, const char* arg, ... /*, char *const envp[] */);
int execv(const char* pathname, char *const argv[]);
int execvp(const char *file, char *const argv[]);
int execvpe(const char *file, char *const argv[], char *const envp[]);

int fork(void);

int open(const char* pathname, int flags, mode_t mode);
int close(int fd);

ssize_t read(int fd, void* buf, size_t count);
ssize_t write(int fd, const void* buf, size_t size);

_Noreturn void _exit(int status);

// include <sys/syscalls.h> for syscall numbers
long syscall(long number, ...);

char *getcwd(char *buf, size_t size);

#ifdef _AMETHYST_SRC
    #define yield __amethyst_yield
    void yield(void);
#endif

#ifdef __cplusplus
}
#endif

#endif /* _UNISTD_H */

