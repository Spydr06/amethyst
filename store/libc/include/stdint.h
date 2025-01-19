#ifndef _STDINT_H
#define _STDINT_H

#ifdef __cplusplus
extern "C" {
#endif 

#include <bits/alltypes.h>

#if __WORDSIZE == 64
    // thanks Tsoding...
    #define __INT64_CLITERAL(c)    c##L
    #define __UINT64_CLITERAL(c)   c##UL
#else
    #define __INT64_CLITERAL(c)    c##LL
    #define __UINT64_CLITERAL(c)   c##ULL
#endif

#define INT8_MIN    (-127-1)
#define INT8_MAX    (127)
#define UINT8_MAX   (255)

#define INT16_MIN   (-32767-1)
#define INT16_MAX   (32767)
#define UINT16_MAX  (65535)

#define INT32_MIN   (-2147483647-1)
#define INT32_MAX   (2147483647)
#define UINT32_MAX  (4294967295U)

#define INT64_MIN   (-__INT64_CLITERAL(9223372036854775807)-1)
#define INT64_MAX   (__INT64_CLITERAL(9223372036854775807))
#define UINT64_MAX  (__UINT64_CLITERAL(18446744073709551615))

#define INTMAX_MIN INT64_MIN
#define INTMAX_MAX INT64_MAX
#define UINTMAX_MAX UINT64_MAX

#if __WORDSIZE == 64
    #define PTRDIFF_MIN INT64_MIN
    #define PTRDIFF_MAX INT64_MAX

    #define SIZE_MAX UINT64_MAX
#else
    #define PTRDIFF_MIN INT32_MIN
    #define PTRDIFF_MAX INT32_MAX
    
    #define SIZE_MAX UINT32_MAX
#endif

#ifdef __cplusplus
}
#endif

#endif /* _STDINT_H */

