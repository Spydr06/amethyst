#ifndef _AMETHYST_LIBC_STRING_H
#define _AMETHYST_LIBC_STRING_H

#include <stddef.h>
#include <stdint.h>

void* memset(void* s, int c, size_t n);
void* memcpy(void* dst, const void* src, size_t n);
void* memmove(void* dest, const void *src, size_t n);

int memcmp(const void* vl, const void* vr, size_t n);

char* reverse(char* str, size_t len);
char* utoa(uint64_t num, char* str, int base);
char* itoa(int64_t num, char* str, int base);

size_t strlen(const char* s);
size_t strnlen(const char* s, size_t maxlen);
char* strcpy(char* dest, const char* src);

int strncmp(const char* _l, const char* _r, size_t n);

char* strerror(int errnum);

#endif /* _AMETHYST_LIBC_STRING_H */

