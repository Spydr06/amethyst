#ifndef _SYS_SYSCALL_H
#define _SYS_SYSCALL_H

#ifdef __cplusplus
extern "C" {
#endif

// Syscall list for the amethyst kernel

#define SYS_read        0
#define SYS_write       1
#define SYS_exit        60
#define SYS_debuglog    255

#ifdef __cplusplus
}
#endif

#endif /* _SYS_SYSCALL_H */

