#ifndef _STDLIB_H
#define _STDLIB_H

#ifdef __cplusplus
extern "C" {
#endif

_Noreturn void exit(int status);
_Noreturn void _Exit(int status);

int atexit(void (*func)(void));

#ifdef __cplusplus
}
#endif

#endif /* _STDLIB_H */

