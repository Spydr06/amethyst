#ifndef _INTERNAL_FORMAT_H
#define _INTERNAL_FORMAT_H

#include <stdarg.h>
#include <bits/alltypes.h>

#define FMT_CHAR '%'

typedef ssize_t (*fmtprinter_t)(void *restrict, const char *restrict, size_t);

int printf_impl(fmtprinter_t printer, void* userp, const char *restrict format, va_list ap);

#endif /* _INTERNAL_FORMAT_H */

