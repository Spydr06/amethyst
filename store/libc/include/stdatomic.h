#ifndef _STDATOMIC_H
#define _STDATOMIC_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stdint.h>

typedef _Atomic bool atomic_bool;

typedef _Atomic char atomic_char;
typedef _Atomic signed char atomic_schar;
typedef _Atomic unsigned char atomic_uchar;

typedef _Atomic short atomic_short;
typedef _Atomic unsigned short atomic_ushort;

typedef _Atomic int atomic_int;
typedef _Atomic unsigned int atomic_uint;

typedef _Atomic long atomic_long;
typedef _Atomic unsigned long atomic_ulong;

typedef _Atomic long long atomic_llong;
typedef _Atomic unsigned long long atomic_ullong;

typedef _Atomic intptr_t atomic_intptr_t;
typedef _Atomic uintptr_t atomic_uintptr_t;
typedef _Atomic size_t atomic_size_t;
typedef _Atomic ptrdiff_t atomic_ptrdiff_t;
typedef _Atomic intmax_t atomic_intmax_t;
typedef _Atomic uintmax_t atomic_uintmax_t;

#ifdef __cplusplus
}
#endif

#endif /* _STDATOMIC_H */

