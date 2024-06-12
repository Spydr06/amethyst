#ifndef _AMETHYST_SYS_HASH_H
#define _AMETHYST_SYS_HASH_H

#include <stdint.h>

#define FNV1PRIME  0x100000001b3ull
#define FNV1OFFSET 0xcbf29ce484222325ull

// Reference:
// https://en.wikipedia.org/wiki/Fowler%E2%80%93Noll%E2%80%93Vo_hash_function
//
static inline uint64_t fnv1ahash(const void *buffer, size_t size) {
	const uint8_t *ptr = buffer;
	const uint8_t *top = ptr + size;
	uint64_t h = FNV1OFFSET;

	while (ptr < top) {
		h ^= *ptr++;
		h *= FNV1PRIME;
	}

	return h;
}



#endif /* _AMETHYST_SYS_HASH_H */

