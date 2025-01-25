#include <stdio.h>

#include <internal/file.h>

#include <sys/param.h>
#include <string.h>
#include <unistd.h>

size_t fread(void *restrict ptr, size_t nmemb, size_t size, FILE *restrict f) {
    unsigned char *dst = ptr;
    size_t len = size * nmemb;
    size_t l = len;
    size_t k;

    if(!size)
        nmemb = 0;

    if(f->rpos != f->rend) {
        // read from buffer
        k = MIN((size_t) (f->rend - f->rpos), l);
        memcpy(dst, f->rpos, k);
        f->rpos += k;
        dst += k;
        l -= k;
    }

    // read remainder directly
    for(; l; l -= k, dst += k) {
        k = toread(f) ? 0 : f->read(f, dst, l);
        if(!k)
            return (len - l) / size;
    }

    return nmemb;
}

size_t __file_read(FILE *f, unsigned char *buf, size_t len) {
    if(len - !!f->buf_size) {
        ssize_t cnt = read(f->fd, buf, len - !!f->buf_size);
        if(cnt <= 0) {
            f->flags |= cnt ? F_ERR : F_EOF;
            return 0;
        }

        if(cnt < (ssize_t) len - !!f->buf_size) {
            f->flags |= F_EOF;
            return cnt;
        }
    }
    
    ssize_t cnt = read(f->fd, f->buf, f->buf_size);
    if(cnt <= 0) {
        f->flags |= cnt ? F_ERR : F_EOF;
        return 0;
    }

    f->rpos = f->buf;
    f->rend = f->buf + cnt;

    if(f->buf_size)
        buf[len - 1] = *f->rpos++;

    return len;
}

