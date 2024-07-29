#ifndef _AMETHYST_CDEFS_H
#define _AMETHYST_CDEFS_H

#ifdef ASM_FILE
#define _AC(X,Y)	X
#define _AT(T,X)	X
#else
#define __AC(X,Y)	(X##Y)
#define _AC(X,Y)	__AC(X,Y)
#define _AT(T,X)	((T)(X))
#endif

#define _UL(x)		(_AC(x, UL))
#define _ULL(x)		(_AC(x, ULL))

#define _BITUL(x)	(_UL(1) << (x))
#define _BITULL(x)	(_ULL(1) << (x))

#define __FILENAME__ (__builtin_strrchr(__FILE__, '/') ? __builtin_strrchr(__FILE__, '/') + 1 : __FILE__)

#define __section(sect) __attribute__((section (sect)))
#define __always_inline __inline __attribute__ ((__always_inline__))
#define __noreturn _Noreturn

#define __unused __attribute__((unused))
#define __aligned(al) __attribute__((aligned(al)))
#define __interrupt __attribute__ ((interrupt))

#define __low_ptr(ptr) ((void*) (((uintptr_t) (ptr)) + _KERNEL_BASE_)) 

#define __len(arr) (sizeof((arr)) / sizeof(*(arr)))
#define __last(arr) ((arr)[__len(arr) - 1])

#define __align8(ptr) ((void*) (((uintptr_t) (ptr) + 7) & ~7))

#define __tmpvar(var) var##__LINE__

#define __twice(a) a a

#define __either(a, b) ((a) ? (a) : (b))

#ifndef ASM_FILE

extern char _KERNEL_BASE_[];
extern char _end[];

#endif /* ASM_FILE */

#endif /* _AMETHYST_CDEFS_H */

