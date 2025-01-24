#include <stdio.h>

#include <internal/file.h>

int fflush(FILE *f) {
    if(!f) {
        int r = 0;
        r |= fflush(stdout);
        r |= fflush(stderr);

        for(f =*ofl_lock(); f; f = f->next) {
            if(f->wpos != f->wbase)
                r |= fflush(f);
        }
        ofl_unlock();

        return r;
    }

    if(f->wpos != f->wbase) {
        f->write(f, 0, 0);
        if(!f->wpos)
            return EOF;
    }

    if(f->rpos != f->rend)
        f->seek(f, f->rpos - f->rend, SEEK_CUR);

    f->wpos = f->wbase = f->wend = 0;
    f->rpos = f->rend = 0;

    return 0;
}

