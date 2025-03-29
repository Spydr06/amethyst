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

    #define __LONG_MAX 0x7fffffffffffffffL

    typedef unsigned long __jmp_buf[8];
#else
    #error Unsupported architecture!
#endif

#if __cplusplus >= 201103L
    #define NULL nullptr
#elif defined(__cplusplus)
    #define NULL 0L
#else
    #define NULL ((void*) 0)
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

typedef int32_t pid_t;
typedef int32_t tid_t;
typedef int32_t gid_t;
typedef int32_t uid_t;

typedef uint32_t mode_t;
typedef _Int64 ino_t;

typedef _Int64 off_t;
typedef _Int64 blksize_t;

typedef _Int64 dev_t;
typedef _Int64 nlink_t;
typedef _Int64 blkcnt_t;

typedef __builtin_va_list va_list;

typedef struct _mbstate mbstate_t;

typedef struct _fpos64 {
    size_t dummy;
} fpos_t;

struct _IO_FILE;
typedef struct _IO_FILE FILE;

struct _IO_DIR;
typedef struct _IO_DIR DIR;

typedef _Int64 time_t;
typedef _Int64 clock_t;

struct timespec {
    time_t tv_sec;
    time_t tv_nsec;
};

#ifdef __cplusplus
}
#endif

#endif /* _BITS_ALLTYPES_H */

