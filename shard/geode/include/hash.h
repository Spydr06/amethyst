#ifndef _GEODE_HASH_H
#define _GEODE_HASH_H

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

enum geode_hash_alogrithm {
    GEODE_HASH_MD5
};

int geode_hash(enum geode_hash_alogrithm algo, uint8_t *dest, const uint8_t *data, size_t size);

struct md5_context {
    uint64_t size;
    uint32_t buffer[4];
    uint8_t input[64];
    uint8_t digest[16];
};

void md5_init(struct md5_context *ctx);
void md5_update(struct md5_context *ctx, uint8_t *input, size_t input_len);
void md5_finalize(struct md5_context *ctx);
void md5_step(uint32_t *buffer, uint32_t *input);

void md5_string(uint8_t *result, const char *input);
void md5_data(uint8_t *result, const uint8_t *input, size_t size);
void md5_file(FILE *file, uint8_t *result);

#endif /* _GEODE_HASH_H */

