#ifndef _AMETHYST_LIBC_STRING_H
#define _AMETHYST_LIBC_STRING_H

#include <stddef.h>
#include <stdint.h>

void* memset(void* s, int c, size_t n);
void* memcpy(void* dst, const void* src, size_t n);
char* mempcpy(void *restrict dst, const void *restrict src, size_t n);
void* memmove(void* dest, const void *src, size_t n);

int memcmp(const void* vl, const void* vr, size_t n);

char* reverse(char* str, size_t len);
char* utoa(uint64_t num, char* str, int base);
char* itoa(int64_t num, char* str, int base);

char* pretty_size_toa(size_t size, char* buf);

size_t strlen(const char* s);

#define _AMETHYST_STRNLEN_DEFINED 1

size_t strnlen(const char* s, size_t maxlen);
char* strcpy(char* dest, const char* src);
char* strncpy(char* dst, const char* restrict src, size_t dsize);
char* stpncpy(char *restrict dst, const char *restrict src, size_t dsize);

char* strncat(char* restrict d, const char* restrict s, size_t n);

int strncmp(const char* _l, const char* _r, size_t n);
int strcmp(const char* _l, const char* _r);
int strcasecmp(const char* _l, const char *_r);

char* strtok(char* restrict str, const char* restrict delim);

char* strerror(int errnum);

char* strcat(char* restrict dst, const char* restrict src);

#endif /* _AMETHYST_LIBC_STRING_H */

