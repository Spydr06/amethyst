#include <dirent.h>

#include <internal/dir.h>

#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>

DIR *odir_head = NULL;

int closedir(DIR *dir) {
    if(!dir)
        return EINVAL;

    odir_remove(dir);

    int err = close(dir->fd);
    if(dir->buffer)
        free(dir->buffer);

    free(dir);
    return err;
}

int dirfd(DIR *dir) {
    return dir->fd;
}

DIR *fdopendir(int fd) {
    DIR* d = malloc(sizeof(DIR));
    if(!d) {
        errno = ENOMEM;
        return NULL;
    }

    memset(d, 0, sizeof(DIR));

    d->fd = fd;

    odir_add(d);
    return d;
}

DIR *opendir(const char *dirname) {
    if(!dirname) {
        errno = EINVAL;
        return 0;
    }

    int fd = open(dirname, O_RDONLY | O_DIRECTORY, 0666);
    if(fd < 0)
        return 0;

    return fdopendir(fd);
}

struct dirent *readdir(DIR *dir) {
    int err = dentbuf_grow(dir, dir->read_offset);
    if(err) {
        errno = err;
        return NULL;
    }

    if(dir->eod) {
        errno = 0;
        return NULL;
    }

    struct amethyst_dirent* dent = dir->buffer + dir->read_offset;
    dir->read_offset += dent->d_reclen;

    return (struct dirent*) dent;
}


void rewinddir(DIR *dir) {
    lseek(dir->fd, 0, SEEK_SET);
    dir->read_offset = 0l; 
    dir->buffer_size = 0lu;
    dir->eod = false;
}

/*void seekdir(DIR *dir, long off) {
    if(off < 0)
        return;

    size_t buff_off = 0;

    while(off--) {
        struct amethyst_dirent* dent = dir->buffer + buff_off;

    }

    dir->read_offset = (off_t) off;
}

long telldir(DIR *dir) {
    return (long) dir->read_offset;
}
*/
