#ifndef _AMETHYST_ABI_H
#define _AMETHYST_ABI_H

#include <stdint.h>

#define KiB (1024ull)
#define MiB (KiB * 1024ull)
#define GiB (MiB * 1024ull)
#define TiB (GiB * 1024ull)

#define STACK_TOP        ((void*) 0x0000800000000000)
#define INTERPRETER_BASE ((void*) 0x00000beef0000000)

#define TO_DEV(major, minor) ((((major) & 0xfff) << 8) + ((minor) & 0xff))

typedef int32_t pid_t;
typedef int32_t tid_t;
typedef int32_t gid_t;
typedef int32_t uid_t;

typedef uint32_t mode_t;
typedef uint64_t ino_t;

typedef int64_t off_t;
typedef int64_t blksize_t;

typedef uint64_t dev_t;
typedef uint64_t nlink_t;
typedef uint64_t blkcnt_t;

#endif /* _AMETHYST_ABI_H */

