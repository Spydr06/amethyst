#include "hash.h"

#include <errno.h>

int geode_hash(enum geode_hash_alogrithm algo, uint8_t *dest, const uint8_t *data, size_t size) {
    switch(algo) {
    case GEODE_HASH_MD5:
        md5_data(dest, data, size);
        return 0;
    default:
        return EINVAL;
    }
}
