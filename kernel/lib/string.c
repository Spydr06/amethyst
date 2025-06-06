#include "string.h"
#include <kernelio.h>

#include <ctype.h>
#include <stdint.h>

#include <assert.h>
#include <errno.h>

//
// C standard memory manipulation functions
// Implementations paritally taken from musl libc
//

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

char* pretty_size_toa(size_t size, char* buf) {
    long double fract = 0.0;
    const char* suffix;

    if(size < KiB) {
        suffix = "B";
    } 
    else if(size < MiB) {
        suffix = "KiB";
        fract = ((long double) size) / (long double) KiB;
        size /= KiB;
    }
    else if(size < GiB) {
        suffix = "MiB";
        fract = ((long double) size) / (long double) MiB;
        size /= MiB;
    }
    else if(size < TiB) {
        suffix = "GiB";
        fract = ((long double) size) / (long double) GiB;
        size /= GiB;
    }
    else {
        suffix = "TiB";
        fract = ((long double) size) / (long double) TiB;
        size /= TiB;
    }

    utoa(size, buf, 10);

    size_t dec = (size_t) ((fract - (long double) size) * 10.0l);
    if(dec > 0 && fract != 0.0) {
        strcat(buf, ".");
        utoa(dec, buf + strlen(buf), 10);
    }

    strcat(buf, suffix);
    return buf;
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

char* strncat(char* restrict d, const char* restrict s, size_t n) {
    char* orig_d = d;
    d += strlen(d);
    while(n && *s) {
        n--;
        *d++ = *s++;
    }
    *d++ = '\0';
    return orig_d;
}

char* mempcpy(void *restrict dst, const void *restrict src, size_t n) {
    return (char *)memcpy(dst, src, n) + n;
}

char* stpncpy(char *restrict dst, const char *restrict src, size_t dsize) {
     size_t  dlen = strnlen(src, dsize);
     return memset(mempcpy(dst, src, dlen), 0, dsize - dlen);
}

char* strncpy(char* dst, const char* restrict src, size_t dsize) {
    stpncpy(dst, src, dsize);
    return dst;
}

int strcmp(const char* l, const char* r) {
	for (; *l == *r && *l; l++, r++);
	return *(unsigned char *) l - *(unsigned char *) r;
}

int strncmp(const char *_l, const char *_r, size_t n)
{
	const unsigned char *l=(void *)_l, *r=(void *)_r;
	if (!n--) return 0;
	for (; *l && *r && n && *l == *r ; l++, r++, n--);
	return *l - *r;
}

int strcasecmp(const char* l, const char *r) {
    for(; toupper(*l) == toupper(*r) && *l; l++, r++);
    return *(unsigned char*) l - *(unsigned char*) r;
}

char* strcat(char* restrict dest, const char* restrict src) {
    strcpy(dest + strlen(dest), src);
    return dest;
}

char* strstr(const char* haystack, const char* needle) {
    for(size_t i = 0; haystack[i]; i++) {
        bool found = true;
        for(size_t j = 0; needle[j]; j++) {
            if(!needle[j] || haystack[i + j] == needle[j])
                continue;

            found = false;
            break;
        }

        if(found)
            return (char*) (haystack + i);
    }

    return NULL;
}

char* strchrnul(const char* s, int c) {
    c = (uint8_t) c;
    if(!c)
        return (char*) s + strlen(s);

    for(; *s && *(uint8_t*) s != c; s++);
    return (char*) s;
}

char* strchr(const char* s, int c) {
    char* r = strchrnul(s, c);
    return *(uint8_t*) r == (uint8_t) c ? r : 0;
}

char* strrchr(const char* s, int c) {
    return memrchr(s, c, strlen(s) + 1);
}

size_t strspn(const char* s, const char* accept) {
    size_t n = 0;
    for(;; n++)
        if(!s[n] || !strchr(accept, s[n]))
            return n;
}

size_t strcspn(const char* s, const char* reject) {
    size_t n = 0;
    for(;; n++)
        if(!s[n] || strchr(reject, s[n]))
            return n;
}

char* strtok_r(char* restrict s, const char* restrict delim, char** restrict saveptr) {
    if(!s && !(s = *saveptr))
        return NULL;

    s += strspn(s, delim);
    if(!*s)
        return *saveptr = 0;

    *saveptr = s + strcspn(s, delim);
    if(**saveptr)
        *(*saveptr)++ = '\0';
    else
        *saveptr = NULL;

    return s;
}


char* strtok(char* restrict str, const char* restrict delim) {
    static char* saveptr;
    return strtok_r(str, delim, &saveptr);
}

long long strtoll(const char* restrict nptr, char** restrict endptr, int base) {
    assert(base == 10); // TODO: implement other base
    
    long long value = 0;

    while(isdigit(*nptr)) {
        value = value * base + (*nptr - '0');
        nptr++;
    }

    *endptr = (char*) nptr;

    return value;
}

char* strerror(int errnum) {
    switch(errnum) {
    case 0:           return "Success";

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
    case ENOTBLK:     return "Block device required";
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
    case EBADFD:      return "File descriptor in bad state";
    case ENOTSOCK:    return "Not a socket";
    case EDESTADDRREQ:return "Destination address required";
    case EMSGSIZE:    return "Message too large";
    case EPROTOTYPE:  return "Protocol wrong type for socket";
    case ENOPROTOOPT: return "Protocol not available";
    case EPROTONOSUPPORT: return "Protocol not supported";
    case ESOCKTNOSUPPORT: return "Socket type not supported";
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
    case ESHUTDOWN:   return "Cannot send after socket shutdown";
    case EALREADY:    return "Operation already in progress";
    case EINPROGRESS: return "Operation in progress";
    case ESTALE:      return "Stale file handle";
    case EREMOTEIO:   return "Remote I/O error";
    case EDQUOT:      return "Quota exceeded";
    case ENOMEDIUM:   return "No medium found";
    case EMEDIUMTYPE: return "Wrong medium type";
    case EMULTIHOP:   return "Multihop attempted";
    case ENOKEY:      return "Required key not available";
    case EKEYEXPIRED: return "Key has expired";
    case EKEYREVOKED: return "Key has been revoked";
    case EKEYREJECTED:return "Key was rejected by service";
    }
    return "Unknown error";
}

