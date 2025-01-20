#include <stdio.h>
#include <internal/file.h>

#include <stdlib.h>
#include <unistd.h>

int fclose(FILE* f) {
    int r = fflush(f) | f->close(f);
    if(f->flags & F_PERM)
        return r;

    ofl_remove(f);
    free(f);
    return r;
}

int __file_close(FILE *f) {
    return close(f->fd);
}

