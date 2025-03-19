#include <memory.h>
#include <stdint.h>

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

void* memrchr(const void* s, int c, size_t n) {
    const uint8_t* bytes = s;
    while(n--)
        if(bytes[n] == (uint8_t) c)
            return (void*) (bytes + n);
    return NULL;
}
