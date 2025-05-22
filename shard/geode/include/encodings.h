#ifndef _GEODE_ENCODINGS_H
#define _GEODE_ENCODINGS_H

#include <stdint.h>
#include <stddef.h>

#include <lifetime.h>

/**
 * Returns the length of the output buffer required to encode len bytes of
 * data into base32. This is a macro to allow users to define buffer size at
 * compilation time.
 */
#define BASE32_LEN(len)  (((len)/5)*8 + ((len) % 5 ? 8 : 0))

/**
 * Returns the length of the output buffer required to decode a base32 string
 * of len characters. Please note that len must be a multiple of 8 as per
 * definition of a base32 string. This is a macro to allow users to define
 * buffer size at compilation time.
 */
#define UNBASE32_LEN(len)  (((len)/8)*5)

void base32_encode(unsigned char *coded, const uint8_t *src, size_t size);
size_t base32_decode(uint8_t *plain, const unsigned char *coded);

#endif /* _GEODE_ENCODINGS_H */

