#ifndef _AMETHYST_LIBC_STDIO_H
#define _AMETHYST_LIBC_STDIO_H

#include <stddef.h>
#include <stdarg.h>

#define EOF (-1)

int snprintf(char* str, size_t size, const char* restrict format, ...);

int vsnprintf(char* str, size_t size, const char* restrict format, va_list ap);

#endif /* _AMETHYST_LIBC_STDIO_H */

