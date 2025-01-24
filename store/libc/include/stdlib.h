#ifndef _STDLIB_H
#define _STDLIB_H

#ifdef __cplusplus
extern "C" {
#endif

#include <bits/alltypes.h>

void abort(void) __attribute__((__noreturn__));

void exit(int status) __attribute__((__noreturn__));
void _Exit(int status) __attribute__((__noreturn__));

int atexit(void (*func)(void));

void *malloc(size_t size);
void *calloc(size_t nmemb, size_t size);
void *realloc(void* ptr, size_t size);
void free(void *ptr);

#ifdef _AMETHYST_SOURCE
    int getargc(void);
    char* getargv(int argc);
#endif

#ifdef __cplusplus
}
#endif

#endif /* _STDLIB_H */

