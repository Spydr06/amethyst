#include "string.h"

#include <stdint.h>

void* memset(void* s, int c, size_t n) {
    uint8_t* p = s;
    while(n--)
        *p++ = (uint8_t) c;
    return s;
}
