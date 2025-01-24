#ifndef _INTERNAL_FILE_H
#define _INTERNAL_FILE_H

#include <stdio.h>
#include <string.h>
#include <bits/alltypes.h>

#define MAX_UNGET 8

enum file_flags {
    F_PERM = 0x01,
    F_NORD = 0x02,
    F_NOWR = 0x04,
    F_ERR  = 0x08,
    F_EOF  = 0x10
};

struct _IO_FILE {
    int fd;
    enum file_flags flags;
    int lbf;

    FILE *prev, *next;

    unsigned char* buf;
    size_t buf_size;

    unsigned char *wbase, *wpos, *wend;
    unsigned char *rbase, *rpos, *rend;

    // callbacks
    size_t (*read)(FILE*, unsigned char*, size_t);
    size_t (*write)(FILE*, const unsigned char*, size_t);
    off_t (*seek)(FILE*, off_t, int);
    int (*close)(FILE*);
};

__attribute__((visibility("hidden")))
extern FILE *ofl_head;

int _fmodeflags(const char* mode);
FILE *_fdopen(int fd, const char* mode);

size_t __file_read(FILE *f, unsigned char *buf, size_t len);
size_t __file_write(FILE *f, const unsigned char *buf, size_t len);
off_t __file_seek(FILE *f, off_t off, int action);
int __file_close(FILE *f);

static inline int towrite(FILE *f) {
    // f->mode |= f->mode-1;
    if(f->flags & F_NOWR) {
        f->flags |= F_ERR;
        return EOF;
    }

    f->rpos = f->rend = 0;

    f->wpos = f->wbase = f->buf;
    f->wend = f->buf + f->buf_size;

    return 0;
}

static inline int toread(FILE *f) {
    // flush buffer
    if(f->wpos != f->wbase)
        f->write(f, NULL, 0);

    f->wpos = f->wbase = f->wend = 0;
    if(f->flags & F_NORD) {
        f->flags |= F_ERR;
        return EOF;
    }

    f->rpos = f->rend = f->buf + f->buf_size;
    return (f->flags & F_EOF) ? EOF : 0;
}

static inline int overflow(FILE *f, int _c) {
    unsigned char c = _c;
    if(!f->wend && towrite(f))
        return EOF;
    if(f->wpos != f->wend && c != f->lbf)
        return *f->wpos++ = c;
    if(f->write(f, &c, sizeof(char)) != 1)
        return EOF;
    return c;
}

static inline FILE **ofl_lock(void) {
    // TODO: lock file list
    return &ofl_head;
}

static inline void ofl_unlock(void) {
    // TODO
}

static inline FILE *ofl_add(FILE *f) {
    FILE **head = ofl_lock();
    f->next = *head;
    if(*head)
        (*head)->prev = f;
    *head = f;
    ofl_unlock();
    return f;
}

static inline void ofl_remove(FILE *f) {
    FILE **head = ofl_lock();
    if(f->prev)
        f->prev->next = f->next;
    if(f->next)
        f->next->prev = f->prev;
    if(*head == f)
        *head = f->next;
    ofl_unlock();
}
#endif /* _INTERNAL_FILE_H */

