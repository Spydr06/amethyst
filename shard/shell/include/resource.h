#ifndef _SHARD_SHELL_RESOURCE_H
#define _SHARD_SHELL_RESOURCE_H

#include <stddef.h>
#include <stdint.h>

#include <libshard.h>

struct shell_resource {
    void* userp;

    uint32_t line, column;
    size_t offset;

    void (*dtor)(void* userp);
    int (*getc)(struct shell_resource*);
    int (*ungetc)(struct shell_resource*, int);
};

enum shell_resource_string_flags {
    RESOURCE_STRING_AUTOFREE = 1
};

int resource_from_file(const char* filepath, const char* restrict mode, struct shell_resource* resource);
int resource_from_string(const char* string, enum shell_resource_string_flags flags, struct shell_resource* resource);

void resource_free(struct shell_resource* resource);

int resource_open(const char* filepath, struct shard_source* dest, const char* restrict mode);

#endif /* _SHARD_SHELL_RESOURCE_H */

