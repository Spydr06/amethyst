#ifndef _AMETHYST_SYSCALL_H
#define _AMETHYST_SYSCALL_H

// Syscall list for the amethyst kernel

#define SYS_read        0
#define SYS_write       1
#define SYS_open        2
#define SYS_close       3
#define SYS_stat        4
#define SYS_fstat       5
#define SYS_mkdir       6
#define SYS_getcwd      7
#define SYS_getdents    8
#define SYS_mmap        9
#define SYS_munmap      11
#define SYS_brk         12
#define SYS_mount       13
#define SYS_umount      14
#define SYS_ioctl       16
#define SYS_yield       24
#define SYS_nanosleep   25
#define SYS_fork        57
#define SYS_execve      59
#define SYS_exit        60
#define SYS_knldebug    255

#endif /* _AMETHYST_SYSCALL_H */

