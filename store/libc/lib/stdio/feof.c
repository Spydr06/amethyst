#include <stdio.h>

#include "internal/file.h"

int feof(FILE *stream) {
    return !!(stream->flags & F_EOF);
}

