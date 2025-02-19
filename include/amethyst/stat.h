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
    dev_t st_dev;
    ino_t st_ino;
    nlink_t st_nlink;
    mode_t st_mode;
    uid_t st_uid;
    gid_t st_gid;
    uint32_t __pad;
    dev_t st_rdev;
    off_t st_size;
    blksize_t st_blksize;
    blkcnt_t st_blocks;
    struct timespec st_atim;
    struct timespec st_mtim;
    struct timespec st_ctim;
    long __reserved[3];
};

#ifdef __cplusplus
}
#endif

#endif /* _AMETHYST_STAT_H */

