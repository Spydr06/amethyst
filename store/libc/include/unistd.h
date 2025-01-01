#ifndef _UNISTD_H
#define _UNISTD_H

#include <bits/alltypes.h>

#ifdef __cplusplus
extern "C" {
#endif

int open(const char* pathname, int flags, mode_t mode);
int close(int fd);

ssize_t read(int fd, void* buf, size_t count);
ssize_t write(int fd, const void* buf, size_t size);

_Noreturn void _exit(int status);

// include <sys/syscalls.h> for syscall numbers
long syscall(long number, ...);

#ifdef __cplusplus
}
#endif

#endif /* _UNISTD_H */

