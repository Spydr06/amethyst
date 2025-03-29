#ifndef _LIBC_INTERNAL_DIR_H
#define _LIBC_INTERNAL_DIR_H

#include <bits/alltypes.h>
#include <sys/syscall.h>

#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

struct _IO_DIR {
    int fd;

    DIR *next, *prev;

    void* buffer;
    size_t buffer_alloc;
    size_t buffer_size;

    size_t read_offset;

    bool eod;
};

__attribute__((visibility("hidden")))
extern DIR *odir_head;

static inline int dentbuf_grow(DIR* d, size_t min) {
    if(min < d->buffer_size)
        return 0;

    if(min >= d->buffer_alloc) {
        size_t new_alloc = (d->buffer_alloc * 3) / 2;
        d->buffer_alloc = new_alloc > min ? new_alloc : BUFSIZ;

        void* new_buffer = realloc(d->buffer, d->buffer_alloc);
        if(!new_buffer)
            return ENOMEM;
        else
            d->buffer = new_buffer;
    }

    long read = syscall(SYS_getdents, d->fd, d->buffer + d->buffer_size, d->buffer_alloc - d->buffer_size);
    if(read < 0)
        return errno;
    else if(read < min) {
        d->eod = true;
    }

    d->buffer_size += read;
    return 0;
}

static inline DIR **odir_lock(void) {
    // TODO: lock dir list
    return &odir_head;
}

static inline void odir_unlock(void) {
    // TODO
}

static inline DIR *odir_add(DIR *d) {
    DIR **head = odir_lock();
    d->next = *head;
    if(*head)
        (*head)->prev = d;
    *head = d;
    odir_unlock();
    return d;
}

static inline void odir_remove(DIR* d) {
    DIR **head = odir_lock();
    if(d->prev)
        d->prev->next = d->next;
    if(d->next)
        d->next->prev = d->prev;
    if(*head == d)
        *head = d->next;
    odir_unlock();
}

#endif /* _LIBC_INTERNAL_DIR_H */
