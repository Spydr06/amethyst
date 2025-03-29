#ifndef _AMETHYST_DIRENT_H
#define _AMETHYST_DIRENT_H

#ifdef _AMETHYST_KERNEL_SRC
    #include <abi.h>
#else
    #include <bits/alltypes.h>
#endif

#define DT_REG 1
#define DT_DIR 2
#define DT_FIFO 3
#define DT_SOCK 4
#define DT_LNK 5
#define DT_BLK 6
#define DT_CHR 7
#define DT_UNKNOWN 0

struct amethyst_dirent {
    ino_t d_ino;
    off_t d_off;
    uint16_t d_reclen;
    uint8_t d_type;
    char d_name[1024];
};

#endif /* _AMETHYST_DIRENT_H */

