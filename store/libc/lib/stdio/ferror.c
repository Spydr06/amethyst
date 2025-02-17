#include <stdio.h>

#include "internal/file.h"

void clearerr(FILE *stream) {
    stream->flags &= ~(F_EOF | F_ERR);
}

int ferror(FILE *stream) {
    return !!(stream->flags & F_ERR);
}
