#ifndef _AMETHYST_ERRNO_H
#define _AMETHYST_ERRNO_H

#include <cpu/cpu.h>
#include <sys/thread.h>

#define EPERM            1
#define ENOENT           2
#define ESRCH            3
#define EINTR            4
#define EIO              5
#define ENXIO            6
#define E2BIG            7
#define ENOEXEC          8
#define EBADF            9
#define ECHILD          10
#define EAGAIN          11
#define ENOMEM          12
#define EACCES          13
#define EFAULT          14
#define ENOTBLK         15
#define EBUSY           16
#define EEXIST          17
#define EXDEV           18
#define ENODEV          19
#define ENOTDIR         20
#define EISDIR          21
#define EINVAL          22
#define ENFILE          23
#define EMFILE          24
#define ENOTTY          25
#define ETXTBSY         26
#define EFBIG           27
#define ENOSPC          28
#define ESPIPE          29
#define EROFS           30
#define EMLINK          31
#define EPIPE           32
#define EDOM            33
#define ERANGE          34
#define ENOMSG          35
#define EIDRM           36
#define ECHRNG          37
#define EL2NSYNC        38
#define EL3HLT          39
#define EL3RST          40
#define ELNRNG          41
#define EUNATCH         42
#define ENOCSI          43
#define EL2HLT          44
#define EDEADLK         45
#define ENOLCK          46
#define EBADE           50
#define EBADR           51
#define EXFULL          52
#define ENOANO          53
#define EBADRQC         54
#define EBADSLT         55
#define EDEADLOCK       56
#define EBFONT          59
#define ENOSTR          60
#define ENODATA         61
#define ETIME           62
#define ENOSR           63
#define ENONET          64
#define ENOPKG          65
#define EREMOTE         66
#define ENOLINK         67
#define EADV            68
#define ESRMNT          69
#define ECOMM           70
#define EPROTO          71
#define EDOTDOT         73
#define EMULTIHOP       74
#define EBADMSG         77
#define ENAMETOOLONG    78
#define EOVERFLOW       79
#define ENOTUNIQ        80
#define EBADFD          81
#define EREMCHG         82
#define ELIBACC         83
#define ELIBBAD         84
#define ELIBSCN         85
#define ELIBMAX         86
#define ELIBEXEC        87
#define EILSEQ          88
#define ENOSYS          89
#define ELOOP           90
#define ERESTART        91
#define ESTRPIPE        92
#define ENOTEMPTY       93
#define EUSERS          94
#define ENOTSOCK        95
#define EDESTADDRREQ    96
#define EMSGSIZE        97
#define EPROTOTYPE      98
#define ENOPROTOOPT     99
#define EPROTONOSUPPORT 120
#define ESOCKTNOSUPPORT 121
#define EOPNOTSUPP      122
#define ENOTSUP         EOPNOTSUPP
#define EPFNOSUPPORT    123
#define EAFNOSUPPORT    124
#define EADDRINUSE      125
#define EADDRNOTAVAIL   126
#define ENETDOWN        127
#define ENETUNREACH     128
#define ENETRESET       129
#define ECONNABORTED    130
#define ECONNRESET      131
#define ENOBUFS         132
#define EISCONN         133
#define ENOTCONN        134
#define EUCLEAN         135
#define ENOTNAM         137
#define ENAVAIL         138
#define EISNAM          139
#define EREMOTEIO       140
#define ESHUTDOWN       143
#define ETOOMANYREFS    144
#define ETIMEDOUT       145
#define ECONNREFUSED    146
#define EHOSTDOWN       147
#define EHOSTUNREACH    148
#define EWOULDBLOCK     EAGAIN
#define EALREADY        149
#define EINPROGRESS     150
#define ESTALE          151
#define ECANCELED       158
#define ENOMEDIUM       159
#define EMEDIUMTYPE     160
#define ENOKEY          161
#define EKEYEXPIRED     162
#define EKEYREVOKED     163
#define EKEYREJECTED    164
#define EOWNERDEAD      165
#define ENOTRECOVERABLE 166
#define ERFKILL         167
#define EHWPOISON       168
#define EDQUOT          1133

#define errno (*_errno_location())

static inline int* _errno_location(void) {
    struct thread* thread = current_thread();
    return thread ? &thread->_errno : &_cpu()->_errno;
}

#endif /* _AMETHYST_ERRNO_H */

