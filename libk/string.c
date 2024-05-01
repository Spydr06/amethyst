#include "string.h"

#include <ctype.h>
#include <stdint.h>

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

