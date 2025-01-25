#include "unistd.h"
#include <stdio.h>

#include <internal/file.h>
#include <sys/param.h>

#include <string.h>

static size_t fwrite_impl(const unsigned char *restrict s, size_t l, FILE *restrict f) {
    size_t i = 0;

    if(!f->wend && towrite(f))
        return 0;

    if(l > (size_t) (f->wend - f->wpos))
        return f->write(f, s, l);

    if(f->lbf >= 0) {
        // Match /^(.*\n|)/

        for(i = l; i && s[i - 1] != '\n'; i--);
        if(i) {
            size_t n = f->write(f, s, i);
            if(n < i)
                return n;
            s += i;
            l -= i;
        }
    }

    memcpy(f->wpos, s, l);
    f->wpos += l;
    return l + i;
}

size_t fwrite(const void *restrict ptr, size_t nmemb, size_t size, FILE *restrict stream) {
    size_t l = size * nmemb;
    if(!size)
        nmemb = 0;
    size_t k = fwrite_impl(ptr, l, stream);
    return k == l ? nmemb : k / size;
}

size_t __file_write(FILE *f, const unsigned char *buf, size_t len) {
    if(f->wpos > f->wbase) {
        ssize_t cnt = write(f->fd, f->wbase, f->wpos - f->wbase);
        f->wpos = MAX(f->wbase, f->wpos - cnt);
        f->wend = f->buf + f->buf_size;

        if(cnt < 0) {
            f->wpos = f->wbase = f->wend = 0;
            f->flags |= F_ERR;
            return 0;
        }
    }
    
    if(!buf || !len)
        return 0;

    ssize_t cnt = write(f->fd, buf, len);
    if(cnt < 0)
        f->flags |= F_ERR;
    return MAX(cnt, 0);
}

int fputc(int c, FILE *restrict stream) {
    if(c != stream->lbf && stream->wpos != stream->wend)
        return *stream->wpos++ = (unsigned char) c;
    else
        return overflow(stream, c);
}

int fputs(const char *restrict s, FILE *restrict stream) {
    size_t l = strlen(s);
    return (fwrite(s, l, sizeof(char), stream) == l) - 1;
}

int puts(const char *restrict s) {
    return -(fputs(s, stdout) < 0 || fputc('\n', stdout) < 0);
}

