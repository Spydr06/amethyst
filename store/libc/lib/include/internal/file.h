#ifndef _INTERNAL_FILE_H
#define _INTERNAL_FILE_H

#include <stdio.h>
#include <bits/alltypes.h>

struct _IO_FILE {
    int fd;
};

int _fmodeflags(const char* mode);
FILE* _fdopen(int fd, const char* mode);

#endif /* _INTERNAL_FILE_H */

