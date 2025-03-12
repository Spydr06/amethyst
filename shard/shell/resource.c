#include "resource.h"

#include <assert.h>
#include <errno.h>
#include <memory.h>
#include <stdio.h>
#include <stdlib.h>

void file_dtor(struct shell_resource* resource) {
    free((char*) resource->resource);

    if(resource->userp)
        fclose(resource->userp);
}

int resource_from_file(const char* filepath, const char* restrict mode, struct shell_resource* resource) {
    memset(resource, 0, sizeof(struct shell_resource));

    FILE* fp = fopen(filepath, mode);
    if(!fp)
        return errno;
    
    if(fseek(fp, 0, SEEK_END) < 0)
        goto error;

    long file_size = ftell(fp);
    if(file_size < 0)
        goto error;
    
    if(fseek(fp, 0, SEEK_SET) < 0)
        goto error;

    char* buffer = malloc(file_size + 1);
    size_t read = fread(buffer, sizeof(char), file_size, fp);
    buffer[read] = '\0';

    resource->resource = buffer;
    resource->dtor = file_dtor;

    return 0;

error:
    int err = errno;
    fclose(fp);
    return err;
}

static void string_dtor(struct shell_resource* resource) {
    free((char*) resource->resource);
}

int resource_from_string(const char* string, enum shell_resource_string_flags flags, struct shell_resource* resource) {
    memset(resource, 0, sizeof(struct shell_resource));

    if(flags & RESOURCE_STRING_AUTOFREE)
        resource->dtor = string_dtor;

    resource->resource = string;

    return 0;
}

void resource_free(struct shell_resource* resource) {
    if(resource->dtor)
        resource->dtor(resource->userp);
}

