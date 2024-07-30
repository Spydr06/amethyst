#ifndef _AMETHYST_ABI_H
#define _AMETHYST_ABI_H

#include <stdint.h>
#include <time.h>

#define KiB (1024ull)
#define MiB (KiB * 1024ull)
#define GiB (MiB * 1024ull)
#define TiB (GiB * 1024ull)

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

struct stat {
    dev_t dev;
    ino_t ino;
    nlink_t nlink;
    mode_t mode;
    uid_t uid;
    gid_t gid;
    uint32_t __pad;
    dev_t rdev;
    off_t size;
    blksize_t blksize;
    blkcnt_t blocks;
    struct timespec atim;
    struct timespec mtim;
    struct timespec ctim;
    long __reserved[3];
};

struct dent {
    ino_t d_ino;
    off_t d_off;
    uint16_t d_reclen;
    uint8_t d_type;
    char d_name[1024];
};

#endif /* _AMETHYST_ABI_H */

