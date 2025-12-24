#ifndef _AMETHYST_LIBC_MEMORY_H
#define _AMETHYST_LIBC_MEMORY_H

#include <stddef.h>

void* memset(void* s, int c, size_t n);
void* memcpy(void* dst, const void* src, size_t n);
char* mempcpy(void *restrict dst, const void *restrict src, size_t n);
void* memmove(void* dest, const void *src, size_t n);

int memcmp(const void* vl, const void* vr, size_t n);
void* memrchr(const void* s, int c, size_t n);

#endif /* _AMETHYST_LIBC_MEMORY_H */

