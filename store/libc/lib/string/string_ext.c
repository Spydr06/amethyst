#include <string.h>

size_t strnlen(const char *s, size_t n) {
	const char *p = memchr(s, 0, n);
	return p ? (size_t) (p - s) : n;
}

char* stpcpy(char* restrict d, const char* restrict s) {
    for(; (*d = *s); s++, d++);
    return d;
}

char* stpncpy(char* restrict d, const char* restrict s, size_t n) {
    for(; n && (*d = *s); n--, s++, d++);
    memset(d, 0, n);
    return d;
}

char* strchrnul(const char* s, int c) {
    c = (uint8_t) c;
    if(!c)
        return (char*) s + strlen(s);

    for(; *s && *(uint8_t*) s != c; s++);
    return (char*) s;
}

char* strtok_r(char* restrict s, const char* restrict delim, char** restrict saveptr) {
    if(!s && !(s = *saveptr))
        return NULL;

    s += strspn(s, delim);
    if(!*s)
        return *saveptr = 0;

    *saveptr = s + strcspn(s, delim);
    if(**saveptr)
        *(*saveptr)++ = '\0';
    else
        *saveptr = NULL;

    return s;
}

