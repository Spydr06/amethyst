#include <internal/file.h>

#include <bits/fcntl.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>

FILE *ofl_head = NULL;

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
    if(!strchr("rwa", *mode)) {
        errno = EINVAL;
        return NULL;
    }

    FILE* f = malloc(sizeof(FILE) + BUFSIZ + MAX_UNGET);
    if(!f) {
        errno = ENOMEM;
        return NULL;
    }
    
    memset(f, 0, sizeof(FILE));

    f->fd = fd;
    f->buf = (unsigned char*) f + sizeof(FILE) + MAX_UNGET;
    f->buf_size = BUFSIZ;
    f->lbf = EOF;

    f->read = __file_read;
    f->write = __file_write;
    f->seek = __file_seek;
    f->close = __file_close;

    return ofl_add(f);
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

