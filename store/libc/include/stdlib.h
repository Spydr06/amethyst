#ifndef _STDLIB_H
#define _STDLIB_H

#ifdef __cplusplus
extern "C" {
#endif

_Noreturn void exit(int status);
_Noreturn void _Exit(int status);

int atexit(void (*func)(void));

#ifdef _AMETHYST_SOURCE
    int getargc(void);
    char* getargv(int argc);
#endif

#ifdef __cplusplus
}
#endif

#endif /* _STDLIB_H */

