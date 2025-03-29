#ifndef _DIRENT_H
#define _DIRENT_H

#include <bits/alltypes.h>

struct dirent {
    ino_t d_ino;
    off_t d_off;
    uint16_t d_reclen;
    uint8_t d_type;
    char d_name[];
};

int closedir(DIR *);
int dirfd(DIR *);

DIR *fdopendir(int);
DIR *opendir(const char *);

struct dirent *readdir(DIR *);

void rewinddir(DIR *);
void seekdir(DIR *, long);
long telldir(DIR *);

#endif /* _DIRENT_H */

