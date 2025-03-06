#include "resource.h"

#include <assert.h>
#include <errno.h>
#include <memory.h>
#include <stdio.h>
#include <stdlib.h>

static void fclose_wrapper(void* fp) {
    assert(fclose((FILE*) fp) == 0);
}

static int fgetc_wrapper(struct shell_resource* resource) {
    return fgetc(resource->userp);
}

static int ungetc_wrapper(struct shell_resource* resource, int c) {
    return ungetc(c, resource->userp);
}

int resource_from_file(const char* filepath, const char* restrict mode, struct shell_resource* resource) {
    memset(resource, 0, sizeof(struct shell_resource));

    FILE* fp = fopen(filepath, mode);
    if(!fp)
        return errno;
    
    resource->userp = (void*) fp;
    resource->dtor = fclose_wrapper;
    resource->getc = fgetc_wrapper;
    resource->ungetc = ungetc_wrapper;
    return 0;
}

static int string_getc(struct shell_resource* resource) {
    const char* str = resource->userp;
    if(str[resource->offset])
        return str[resource->offset++];
    return EOF;
}

static int string_ungetc(struct shell_resource* resource, int c) {
    if(resource->offset == 0)
        return EOF;
    
    char* str = resource->userp;
    return str[resource->offset--] = c;
}

int resource_from_string(const char* string, enum shell_resource_string_flags flags, struct shell_resource* resource) {
    memset(resource, 0, sizeof(struct shell_resource));

    if(flags & RESOURCE_STRING_AUTOFREE)
        resource->dtor = free;

    resource->userp = (void*) string;

    resource->getc = string_getc;
    resource->ungetc = string_ungetc;

    return 0;
}

void resource_free(struct shell_resource* resource) {
    if(resource->dtor)
        resource->dtor(resource->userp);
}

void resource_rewind(struct shell_resource* resource) {
    resource->line   = 0;
    resource->column = 0;
    resource->offset = 0;
}
