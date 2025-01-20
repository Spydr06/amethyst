#include <stdio.h>

#include <internal/file.h>

int fseek(FILE *stream, long off, int whence) {
    return 0;
}

off_t __file_seek(FILE *f, off_t off, int action) {
    return 0;
}

