#ifndef _AMETHYST_LIBC_STRING_H
#define _AMETHYST_LIBC_STRING_H

#include <stddef.h>
#include <stdint.h>

void* memset(void* s, int c, size_t n);

char* reverse(char* str, size_t len);
char* utoa(uint64_t num, char* str, int base);
char* itoa(int64_t num, char* str, int base);

size_t strlen(const char*);

#endif /* _AMETHYST_LIBC_STRING_H */

