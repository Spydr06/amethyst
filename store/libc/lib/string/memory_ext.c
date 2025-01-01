#include <string.h>

#include <stdbool.h>

void* memrchr(const void* s, int c, size_t n) {
    const uint8_t* bytes = s;
    while(n--)
        if(bytes[n] == (uint8_t) c)
            return (void*) (bytes + n);
    return NULL;
}

void* memccpy(void* restrict d, const void* restrict s, int c, size_t n) {
    uint8_t* dst = d;
    const uint8_t* src = s;

    for(; n && (*dst = *src) != c; n--, src++, dst++);
    if(n)
        return dst + 1;
    return NULL;
}

void* memmmem(const void* haystack, size_t haystacklen, const void* needle, size_t needlelen) {
    const uint8_t* hs = haystack;
    const uint8_t* nd = needle;

    for(size_t i = 0; i < haystacklen; i++) {
        bool found = true;

        for(size_t j = 0; j < needlelen; j++) {
            if(i + j >= haystacklen || hs[i + j] != nd[j]) {
                found = false;
                break;
            }
        }

        if(found)
            return (void*) (hs + i);
    }

    return NULL;
}

void* mempcpy(void* d, const void* s, size_t n) {
    return (char*) memcpy(d, s, n) + n;
}

