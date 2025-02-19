#ifndef _AMETHYST_STAT_H
#define _AMETHYST_STAT_H

#ifdef __cplusplus
extern "C" {
#endif

#ifdef _AMETHYST_KERNEL_SRC
    #include <abi.h>
#else
    #include <bits/alltypes.h>
#endif

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

#ifdef __cplusplus
}
#endif

#endif /* _AMETHYST_STAT_H */

