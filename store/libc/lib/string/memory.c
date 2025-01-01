#include <string.h>

#include <stdint.h>

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpedantic"

#define ld(p, T) ({ T _v; __builtin_memcpy(&_v, (p), sizeof(_v)); (p) += sizeof(_v); _v; })
#define st(p, v) do { __builtin_memcpy((p), &(v), sizeof((v))); (p) += sizeof((v)); } while(0)

// optimization idea from mlibc
void* memcpy(void* restrict d, const void* restrict s, size_t n) {
    uint8_t* dst = d;
    const uint8_t* src = s;

    for(; n >= 64; n -= 64) {
        uint64_t w[8] = {
            ld(src, uint64_t), 
            ld(src, uint64_t), 
            ld(src, uint64_t), 
            ld(src, uint64_t), 
            ld(src, uint64_t), 
            ld(src, uint64_t), 
            ld(src, uint64_t), 
            ld(src, uint64_t)
        };

        st(dst, w[0]);
        st(dst, w[1]);
        st(dst, w[2]);
        st(dst, w[3]);
        st(dst, w[4]);
        st(dst, w[5]);
        st(dst, w[6]);
        st(dst, w[7]);
    }

    if(n >= 32) {
        uint64_t w[4] = {
            ld(src, uint64_t),
            ld(src, uint64_t),
            ld(src, uint64_t),
            ld(src, uint64_t)
        };

        st(dst, w[0]);
        st(dst, w[1]);
        st(dst, w[2]);
        st(dst, w[3]);

        n -= 32;
    }

    if(n >= 16) {
        uint64_t w[2] = {
            ld(src, uint64_t),
            ld(src, uint64_t)
        };

        st(dst, w[0]);
        st(dst, w[1]);

        n -= 16;
    }

    if(n >= 8) {
        uint64_t w = ld(src, uint64_t);
        st(dst, w);
        n -= 8;
    }

    if(n >= 4) {
        uint32_t w = ld(src, uint32_t);
        st(dst, w);
        n -= 4;
    }

    if(n >= 2) {
        uint16_t w = ld(src, uint16_t);
        st(dst, w);
        n -= 2;
    }

    if(n)
        *dst = *src;

    return d;
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

// optimization idea from mlibc
void* memset(void* s, int c, size_t n) {
    uint8_t* bytes = s;

    uint64_t pattern64 = (uint64_t) c | (uint64_t) c << 8 | (uint64_t) c << 16 | (uint64_t) c << 24 
        | (uint64_t) c << 32 | (uint64_t) c << 40 | (uint64_t) c << 48 | (uint64_t) c << 56;
    uint32_t pattern32 = (uint32_t) c << 8 | (uint32_t) c << 16 | (uint32_t) c << 24;
    uint16_t pattern16 = (uint16_t) c | (uint16_t) c << 8;

    for(; n >= 64; n -= 64) {
        st(bytes, pattern64);
        st(bytes, pattern64);
        st(bytes, pattern64);
        st(bytes, pattern64);
        st(bytes, pattern64);
        st(bytes, pattern64);
        st(bytes, pattern64);
        st(bytes, pattern64);
    }

    if(n >= 32) {
        st(bytes, pattern64);
        st(bytes, pattern64);
        st(bytes, pattern64);
        st(bytes, pattern64);
        n -= 32;
    }

    if(n >= 16) {
        st(bytes, pattern64);
        st(bytes, pattern64);
        n -= 16;
    }

    if(n >= 8) {
        st(bytes, pattern64);
        n -= 8;
    }

    if(n >= 4) {
        st(bytes, pattern32);
        n -= 4;
    }

    if(n >= 2) {
        st(bytes, pattern16);
        n -= 2;
    }

    if(n)
        *bytes = (uint8_t) c;

    return s;
}

void* memchr(const void* s, int c, size_t n) {
    const uint8_t* bytes = s;
    for(size_t i = 0; i < n; i++)
        if(bytes[i] == (uint8_t) c)
            return (void*) (bytes + i);
    return NULL;
}

int memcmp(const void* s1, const void* s2, size_t n) {
    for(size_t i = 0; i < n; i++) {
        uint8_t a_byte = ((const uint8_t*) s1)[i];
        uint8_t b_byte = ((const uint8_t*) s2)[i];

        if(a_byte < b_byte)
            return -1;
        if(a_byte > b_byte)
            return 1;
    }

    return 0;
}

#pragma GCC diagnostic pop

