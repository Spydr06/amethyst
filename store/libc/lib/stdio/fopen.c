#include <internal/file.h>

#include <bits/fcntl.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>

int _fmodeflags(const char* mode) {
    int fl;
    
    if(strchr(mode, '+'))
        fl = O_RDWR;
    else if(*mode == 'r')
        fl = O_RDONLY;
    else
        fl = O_WRONLY;

    if(strchr(mode, 'x'))
        fl |= O_EXCL;
    if(strchr(mode, 'e'))
        fl |= O_CLOEXEC;
    if(*mode != 'r')
        fl |= O_CREAT;
    if(*mode == 'w')
        fl |= O_TRUNC;
    if(*mode == 'a')
        fl |= O_APPEND;

    return fl;
}

FILE* _fdopen(int fd, const char* mode) {
    // FIXME: unimplemented until heap allocator is in place
    return NULL;
}

FILE* fopen(const char *restrict filename, const char *restrict mode) {
    if(!filename || !mode || !strchr("rwa", *mode)) {
        errno = EINVAL;
        return 0;
    }

    int fl = _fmodeflags(mode);
    int fd = open(filename, fl, 0666);
    if(fd < 0)
        return 0;

    /* TODO: enable cloexec once implemented in kernel
    if(flags & O_CLOEXEC)
        fcntl(fd, F_SETFD, FD_CLOEXEC); */

    FILE* fp = _fdopen(fd, mode);
    if(fp)
        return fp;

    close(fd);
    return NULL;
}

