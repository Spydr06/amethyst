#ifndef _AMETHYST_LIBC_SETJMP_H
#define _AMETHYST_LIBC_SETJMP_H

#include <stdint.h>

typedef intptr_t jmp_buf[5];

#define longjmp(_buf, _val) (__builtin_longjmp((_buf), (_val)))

#define setjmp(_buf) (__builtin_setjmp((_buf)))

#endif /* _AMETHYST_LIBC_SETJMP_H */

