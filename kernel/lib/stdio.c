#include <stdio.h>
#include <string.h>

int snprintf(char* str, size_t size, const char* restrict format, ...) {
    va_list ap;
    va_start(ap, format);
    int res = vsnprintf(str, size, format, ap);
    va_end(ap);
    return res;
}

int vsnprintf(char* str, size_t size, const char* restrict format, va_list ap) {
    // TODO: implement
    (void) ap;
    strncpy(str, format, size);
    return 0;
}

