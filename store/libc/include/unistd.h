#ifndef _UNISTD_H
#define _UNISTD_H

#include <bits/alltypes.h>

#ifdef __cplusplus
extern "C" {
#endif

ssize_t write(int fd, const void* buf, size_t size);

_Noreturn void _exit(int status);

// include <sys/syscalls.h> for syscall numbers
long syscall(long number, ...);

#ifdef __cplusplus
}
#endif

#endif /* _UNISTD_H */
