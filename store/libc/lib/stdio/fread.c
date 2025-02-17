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

int fgetc(FILE *restrict stream) {
    if(stream->rpos != stream->rend)
        return *stream->rpos++;
    else
        return underflow(stream);
}

char *fgets(char *restrict s, int size, FILE *restrict stream) {
    char *p = s;

    if(size == 1)
        return 0;
    else if(size < 0) {
        *s = '\0';
        return NULL;
    }

    size--;

    while(size) {
        if(stream->rpos != stream->rend) {
            unsigned char *z = memchr(stream->rpos, '\n', stream->rend - stream->rpos);
            size_t k = z ? z - stream->rpos + 1 : stream->rend - stream->rpos;
            k = MIN(k, (size_t) size);

            memcpy(p, stream->rpos, k);
            stream->rpos += k;
            p += k;
            size -= k;

            if(z || !size)
                break;
        }

        int c = fgetc(stream);
        if(c < 0) {
            if(p == s || !feof(stream))
                s = NULL;
            break;
        }

        size--;
        if((*p++ = c) == '\n')
            break;
    }

    if(s)
        *p = 0;

    return s;
}

int getc(FILE *restrict stream) {
    return fgetc(stream);
}

int ungetc(int c, FILE *stream) {
    if(c == EOF)
        return c;

    if(!stream->rpos)
        toread(stream);
    if(!stream->rpos || stream->rpos <= stream->buf - MAX_UNGET)
        return EOF;

    *--stream->rpos = c;
    stream->flags &= ~F_EOF;

    return (unsigned char) c;
}

