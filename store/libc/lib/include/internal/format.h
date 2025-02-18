#ifndef _INTERNAL_FORMAT_H
#define _INTERNAL_FORMAT_H

#include <stdarg.h>
#include <bits/alltypes.h>

#define FMT_CHAR '%'

typedef ssize_t (*fmtprinter_t)(void *restrict, const char *restrict, size_t);
typedef ssize_t (*fmtscanner_t)(void *restrict, char *restrict, size_t);

enum length_modifier {
    LMOD_CHAR = 1,    // hh
    LMOD_SHORT,       // h
    LMOD_LONG,        // l, q
    LMOD_LONG_LONG,   // ll
    LMOD_LONG_DOUBLE, // L
    LMOD_INTMAX,      // j
    LMOD_SIZE,        // z, Z
    LMOD_PTRDIFF,     // t
};

int printf_impl(fmtprinter_t printer, void *restrict userp, const char *restrict format, va_list ap);

int scanf_impl(fmtscanner_t scanner, void *restrict userp, const char *restrict format, va_list ap);

void _parse_length_modifier(enum length_modifier *mod, const char *restrict *format);

#endif /* _INTERNAL_FORMAT_H */

