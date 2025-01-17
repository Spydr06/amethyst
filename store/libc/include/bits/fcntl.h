#ifndef _BITS_FCNTL_H
#define _BITS_FCNTL_H

#ifdef __cplusplus
extern "C" {
#endif

#define O_RDONLY 00
#define O_WRONLY 01
#define O_RDWR 02

#define O_CREAT 0100
#define O_EXCL 0200
#define O_NOCTTY 0400
#define O_TRUNC 01000
#define O_APPEND 02000
#define O_NONBLOCK 04000
#define O_DIRECTORY 0100000
#define O_CLOEXEC 02000000

#ifdef __cplusplus
}
#endif

#endif /* _BITS_FCNTL_H */

