#ifndef _BITS_ALLTYPES_H
#define _BITS_ALLTYPES_H

#ifdef __cplusplus
extern "C" {
#endif

// Endianness
#define __LITTLE_ENDIAN 1234
#define __BIT_ENDIAN 4321

#if defined(__x86_64__) || defined(__x86_64)
    #define __BYTE_ORDER 1234

    #define __WORDSIZE 64

    #define _Addr long
    #define _Reg long
    #define _Int64 long
#else
    #error Unsupported architecture!
#endif

typedef signed _Addr intptr_t;
typedef unsigned _Addr uintptr_t;
typedef _Addr ptrdiff_t;

typedef _Addr regoff_t;
typedef _Reg register_t;

typedef signed _Addr ssize_t;
typedef unsigned _Addr size_t;

typedef signed char int8_t;
typedef signed short int16_t;
typedef signed int int32_t;
typedef signed _Int64 int64_t;
typedef signed _Int64 intmax_t;

typedef unsigned char uint8_t;
typedef unsigned short uint16_t;
typedef unsigned int uint32_t;
typedef unsigned _Int64 uint64_t;
typedef unsigned _Int64 uintmax_t;

typedef _Int64 off_t;

typedef uint32_t mode_t;

typedef __builtin_va_list va_list;

typedef struct _mbstate mbstate_t;

typedef struct _fpos64 {
    size_t dummy;
} fpos_t;

struct _IO_FILE;
typedef struct _IO_FILE FILE;

#ifdef __cplusplus
}
#endif

#endif /* _BITS_ALLTYPES_H */

