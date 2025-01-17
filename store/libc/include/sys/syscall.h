#ifndef _SYS_SYSCALL_H
#define _SYS_SYSCALL_H

#ifdef __cplusplus
extern "C" {
#endif

// Syscall list for the amethyst kernel

#define SYS_read        0
#define SYS_write       1
#define SYS_open        2
#define SYS_close       3
#define SYS_mmap        9
#define SYS_munmap      11
#define SYS_brk         12
#define SYS_exit        60
#define SYS_debuglog    255

#ifdef __cplusplus
}
#endif

#endif /* _SYS_SYSCALL_H */

