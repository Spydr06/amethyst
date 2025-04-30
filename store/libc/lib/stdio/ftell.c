#include <errno.h>
#include <limits.h>
#include <stdio.h>

#include <internal/file.h>

off_t __file_tell(FILE *f) {
    off_t pos = f->seek(f, 0, SEEK_CUR);
    if(pos < 0)
        return pos;

    if(f->rend)
        pos += f->rpos - f->rend;
    else if(f->write)
        pos += f->wpos - f->wbase;
    return pos;
}

long ftell(FILE *stream) {
    // TODO: lock & unlock stream
    off_t r = __file_tell(stream);
    if(r > LONG_MAX) {
        errno = EOVERFLOW;
        return -1;
    }

    return (long) r;
}

