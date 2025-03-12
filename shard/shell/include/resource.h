#ifndef _SHARD_SHELL_RESOURCE_H
#define _SHARD_SHELL_RESOURCE_H

#include <stddef.h>
#include <stdint.h>
#include <memory.h>

#include <libshard.h>

struct shell_location {
    uint32_t line;
    uint32_t column;
    size_t offset;
    struct shell_resource* resource;
};

struct shell_resource {
    const char* resource;
    struct shell_location current_loc;

    void* userp;
    void (*dtor)(struct shell_resource* self);
};

enum shell_resource_string_flags {
    RESOURCE_STRING_AUTOFREE = 1
};

int resource_from_file(const char* filepath, const char* restrict mode, struct shell_resource* resource);
int resource_from_string(const char* string, enum shell_resource_string_flags flags, struct shell_resource* resource);

void resource_free(struct shell_resource* resource);

int resource_open(const char* filepath, struct shard_source* dest, const char* restrict mode);

#endif /* _SHARD_SHELL_RESOURCE_H */

