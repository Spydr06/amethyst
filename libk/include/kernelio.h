#ifndef _AMETHYST_KERNELIO_H
#define _AMETHYST_KERNELIO_H

#include <stdarg.h>

typedef void (*kernelio_writer_t)(int);

extern kernelio_writer_t kernelio_writer;

int printk(const char* restrict format, ...) __attribute__((format(printf, 1, 2)));
int fprintk(kernelio_writer_t writer, const char* restrict format, ...) __attribute__((format(printf, 2, 3)));

int vprintk(const char* restrict format, va_list ap);
int vfprintk(kernelio_writer_t writer, const char* restrict format, va_list ap);

#endif /* _AMETHYST_KERNELIO_H */

