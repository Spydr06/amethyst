#define _GNU_SOURCE
#include <string.h>

#include <stdbool.h>

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Winfinite-recursion"

char* strcpy(char* restrict d, const char* restrict s) {
    stpcpy(d, s);
    return d;
}

#pragma GCC diagnostic pop

char* strncpy(char* restrict d, const char* restrict s, size_t n) {
    stpncpy(d, s, n);
    return d;
}

char* strcat(char* restrict d, const char* restrict s) {
    strcpy(d + strlen(d), s);
    return d;
}

char* strncat(char* restrict d, const char* restrict s, size_t n) {
    char* orig_d = d;
    d += strlen(d);
    while(n && *s) {
        n--;
        *d++ = *s++;
    }
    *d++ = '\0';
    return orig_d;
}

int strcmp(const char* s1, const char* s2) {
    for(; *s1 == *s2 && *s1; s1++, s2++);

    return *(uint8_t*) s1 - *(uint8_t*) s2;
}

int strncmp(const char* s1, const char* s2, size_t n) {
    if(!n--)
        return 0;

    for(; *s1 && *s2 && *s1 == *s2; s1++, s2++, n--);
    return *(uint8_t*) s1 - *(uint8_t*) s2;
}

int strcoll(const char* s1, const char* s2) {
    // locales not implemented
    return strcmp(s1, s2);
}

size_t strxfrm(char* restrict d, const char* restrict s, size_t n) {
    // locales not implemented
    size_t l = strlen(s);
    if(n > l)
        strcpy(d, s);
    return l;
}

char* strchr(const char* s, int c) {
    char* r = strchrnul(s, c);
    return *(uint8_t*) r == (uint8_t) c ? r : 0;
}

char* strrchr(const char* s, int c) {
    return memrchr(s, c, strlen(s) + 1);
}

size_t strspn(const char* s, const char* accept) {
    size_t n = 0;
    for(;; n++)
        if(!s[n] || !strchr(accept, s[n]))
            return n;
}

size_t strcspn(const char* s, const char* reject) {
    size_t n = 0;
    for(;; n++)
        if(!s[n] || strchr(reject, s[n]))
            return n;
}

char* strpbrk(const char* s, const char* accept) {
    s += strcspn(s, accept);
    return *s ? (char*) s : 0;
}

char* strstr(const char* haystack, const char* needle) {
    for(size_t i = 0; haystack[i]; i++) {
        bool found = true;
        for(size_t j = 0; needle[j]; j++) {
            if(!needle[j] || haystack[i + j] == needle[j])
                continue;

            found = false;
            break;
        }

        if(found)
            return (char*) (haystack + i);
    }

    return NULL;
}

char* strtok(char* restrict s, const char* restrict delim) {
    static char* saveptr;
    return strtok_r(s, delim, &saveptr);
}

size_t strlen(const char* s) {
    size_t l;
    for(l = 0; *s; l++, s++);
    return l;
}

char* strerror(int _errno);

