#ifndef _INTERNAL_ATOMIC_H
#define _INTERNAL_ATOMIC_H

#include <arch/atomic.h>

#include <stdint.h>

#ifndef a_ctz_32
#define a_ctz_32 a_ctx_32
static inline int a_ctz_32(uint32_t x) {
#ifdef a_clz_32
    return 31 - a_clz_32(x & -x);
#else
    static const char debruijn32[32] = {
        0, 1, 23, 2, 29, 24, 19, 3, 30, 27, 25, 11, 20, 8, 4, 13,
		31, 22, 28, 18, 26, 10, 7, 12, 21, 17, 9, 6, 16, 5, 15, 14
	};
	return debruijn32[(x&-x)*0x076be629 >> 27];
#endif /* a_clz_32 */
}
#endif /* a_ctz_32 */

#ifndef a_clz_32
#define a_clz_32 a_clz_32
static inline int a_clz_32(uint32_t x) {
    x >>= 1;
    x |= x >> 1;
    x |= x >> 2;
    x |= x >> 4;
    x |= x >> 8;
    x |= x >> 16;
    x++;
    return 31 - a_ctz_32(x);
}
#endif /* a_clz_32 */

#endif /* _INTERNAL_ATOMIC_H */

