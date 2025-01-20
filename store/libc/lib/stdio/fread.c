#include <stdio.h>

#include <internal/file.h>

size_t fread(void *restrict ptr, size_t nmemb, size_t size, FILE *restrict stream) {
    return 0;
}

size_t __file_read(FILE *f, unsigned char *buf, size_t len) {
    return 0;
}

