#include "string.h"

#include <ctype.h>
#include <stdint.h>

#include <errno.h>

//
// C standard memory manipulation functions
// Implementations paritally taken from musl libc
//

void* memset(void* s, int c, size_t n) {
    uint8_t* p = s;
    while(n--)
        *p++ = (uint8_t) c;
    return s;
}

void* memcpy(void* dst, const void* src, size_t n) {
    uint8_t* cdst = dst;
    const uint8_t* csrc = src;

    for(size_t i = 0; i < n; i++) {
        cdst[i] = csrc[i];
    }

    return dst;
}

void *memmove(void *dest, const void *src, size_t n)
{
	char *d = dest;
	const char *s = src;

	if (d==s) return d;
	if ((uintptr_t)s-(uintptr_t)d-n <= -2*n) return memcpy(d, s, n);

	if (d<s) {
		if ((uintptr_t)s % sizeof(size_t) == (uintptr_t)d % sizeof(size_t)) {
			while ((uintptr_t)d % sizeof(size_t)) {
				if (!n--) return dest;
				*d++ = *s++;
			}
			for (; n>=sizeof(size_t); n-=sizeof(size_t), d+=sizeof(size_t), s+=sizeof(size_t)) *(size_t *)d = *(size_t *)s;
		}
		for (; n; n--) *d++ = *s++;
	} else {
		if ((uintptr_t)s % sizeof(size_t) == (uintptr_t)d % sizeof(size_t)) {
			while ((uintptr_t)(d+n) % sizeof(size_t)) {
				if (!n--) return dest;
				d[n] = s[n];
			}
			while (n>=sizeof(size_t)) n-=sizeof(size_t), *(size_t *)(d+n) = *(size_t *)(s+n);
		}
		while (n) n--, d[n] = s[n];
	}

	return dest;
}

int memcmp(const void* vl, const void* vr, size_t n) {
    const unsigned char *l = vl, *r = vr;
	for (; n && *l == *r; n--, l++, r++);
	return n ? *l - *r : 0;
}

char* reverse(char* str, size_t len) {
    char tmp;
    for(size_t i = 0; i < len / 2; i++) {
        tmp = str[i];
        str[i] = str[len - i - 1];
        str[len - i - 1] = tmp;
    }
    return str;
}

char* utoa(uint64_t num, char* str, int base) {
    size_t i = 0;
    if(num == 0) {
        str[i++] = '0';
        str[i] = '\0';
        return str;
    }

    while(num != 0) {
        uint64_t rem = num % base;
        if(rem > 9)
            str[i++] = ((rem - 10) + 'a');
        else
            str[i++] = rem + '0';
        num /= base;
    }

    str[i] = '\0';
    reverse(str, i);
    return str;
}

char* itoa(int64_t inum, char* str, int base) {
    size_t i = 0;
    if(inum == 0) {
        str[i++] = '0';
        str[i] = '\0';
        return str;
    }

    uint64_t num = (uint64_t) inum;
    if(inum < 0) {
        *str++ = '-';
        num = (uint64_t) -inum;
    }

    while(num != 0) {
        uint64_t rem = num % base;
        if(rem > 9)
            str[i++] = ((rem - 10) + 'a');
        else
            str[i++] = rem + '0';
        num /= base;
    }

    str[i] = '\0';
    reverse(str, i);
    return str;
}

int atoi(const char* s) {
    int res = 0;
    int sign = 1;
    
    while(isspace(*s)) s++;
    
    if(*s == '-') {
        sign = -1;
        s++;
    }
    else if(*s == '+') {
        sign = 1;
        s++;
    }

    while(*s)
        res = res * 10 + *(s++) - '0';
    return res * sign;
}

size_t strlen(const char* s) {
    size_t l = 0;
    while(*s++) l++;
    return l;
}

size_t strnlen(const char* s, size_t maxlen) {
    size_t l = 0;
    while(maxlen-- > 0 && *s++) l++;
    return l;
}

char* strcpy(char* dest, const char* src) {
    for (; (*dest = *src); src++, dest++);
    return dest;
}

char* strerror(int errnum) {
    switch(errnum) {
    case 0:           return "No error information";

    case EILSEQ:      return "Illegal byte sequence";
    case EDOM:        return "Domain error";
    case ERANGE:      return "Result not representable";

    case ENOTTY:      return "Not a tty";
    case EACCES:      return "Permission denied";
    case EPERM:       return "Operation not permitted";
    case ENOENT:      return "No such file or directory";
    case ESRCH:       return "No such process";
    case EEXIST:      return "File exists";

    case EOVERFLOW:   return "Value too large for data type";
    case ENOSPC:      return "No space left on device";
    case ENOMEM:      return "Out of memory";

    case EBUSY:       return "Resource busy";
    case EINTR:       return "Interrupted system call";
    case EAGAIN:      return "Resource temporarily unavailable";
    case ESPIPE:      return "Invalid seek";

    case EXDEV:       return "Cross-device link";
    case EROFS:       return "Read-only file system";
    case ENOTEMPTY:   return "Directory not empty";

    case ECONNRESET:  return "Connection reset by peer";
    case ETIMEDOUT:   return "Operation timed out";
    case ECONNREFUSED:return "Connection refused";
    case EHOSTDOWN:   return "Host is down";
    case EHOSTUNREACH:return "Host is unreachable";
    case EADDRINUSE:  return "Address in use";

    case EPIPE:       return "Broken pipe";
    case EIO:         return "I/O error";
    case ENXIO:       return "No such device or address";
//    case ENOTBLK:     return "Block device required";
    case ENODEV:      return "No such device";
    case ENOTDIR:     return "Not a directory";
    case EISDIR:      return "Is a directory";
    case ETXTBSY:     return "Text file busy";
    case ENOEXEC:     return "Exec format error";

    case EINVAL:      return "Invalid argument";

    case E2BIG:       return "Argument list too long";
    case ELOOP:       return "Symbolic link loop";
    case ENAMETOOLONG:return "Filename too long";
    case ENFILE:      return "Too many open files in system";
    case EMFILE:      return "No file descriptors available";
    case EBADF:       return "Bad file descriptor";
    case ECHILD:      return "No child process";
    case EFAULT:      return "Bad address";
    case EFBIG:       return "File too large";
    case EMLINK:      return "Too many links";
    case ENOLCK:      return "No locks available";

    case EDEADLK:     return "Resource deadlock would occur";
    case ENOTRECOVERABLE:return "State not recoverable";
    case EOWNERDEAD:  return "Previous owner died";
    case ECANCELED:   return "Operation canceled";
    case ENOSYS:      return "Function not implemented";
    case ENOMSG:      return "No message of desired type";
    case EIDRM:       return "Identifier removed";
    case ENOSTR:      return "Device not a stream";
    case ENODATA:     return "No data available";
    case ETIME:       return "Device timeout";
    case ENOSR:       return "Out of streams resources";
    case ENOLINK:     return "Link has been severed";
    case EPROTO:      return "Protocol error";
    case EBADMSG:     return "Bad message";
//    case EBADFD:      return "File descriptor in bad state";
    case ENOTSOCK:    return "Not a socket";
    case EDESTADDRREQ:return "Destination address required";
    case EMSGSIZE:    return "Message too large";
    case EPROTOTYPE:  return "Protocol wrong type for socket";
    case ENOPROTOOPT: return "Protocol not available";
    case EPROTONOSUPPORT: return "Protocol not supported";
//    case ESOCKTNOSUPPORT: return "Socket type not supported";
    case ENOTSUP:     return "Not supported";
    case EPFNOSUPPORT:return "Protocol family not supported";
    case EAFNOSUPPORT:return "Address family not supported by protocol";
    case EADDRNOTAVAIL: return "Address not available";
    case ENETDOWN:    return "Network is down";
    case ENETUNREACH: return "Network unreachable";
    case ENETRESET:   return "Connection reset by network";
    case ECONNABORTED:return "Connection aborted";
    case ENOBUFS:     return "No buffer space available";
    case EISCONN:     return "Socket is connected";
    case ENOTCONN:    return "Socket not connected";
//    case ESHUTDOWN:   return "Cannot send after socket shutdown";
    case EALREADY:    return "Operation already in progress";
    case EINPROGRESS: return "Operation in progress";
    case ESTALE:      return "Stale file handle";
//    case EREMOTEIO:   return "Remote I/O error";
    case EDQUOT:      return "Quota exceeded";
//    case ENOMEDIUM:   return "No medium found";
//    case EMEDIUMTYPE: return "Wrong medium type";
    case EMULTIHOP:   return "Multihop attempted";
//    case ENOKEY:      return "Required key not available";
//    case EKEYEXPIRED: return "Key has expired";
//    case EKEYREVOKED: return "Key has been revoked";
//    case EKEYREJECTED:return "Key was rejected by service";
    }
    return "Unknown error";
}

