#include <fs_util.h>
#include <context.h>

#include <string.h>
#include <errno.h>

bool filename_has_file_ext(const char* filename, const char* expected_ext) {
    const char* ext = get_file_ext(filename);
    if(!ext)
        return false;

    return strcmp(ext, expected_ext) == 0;
}

const char* get_file_ext(const char* filename) {
    const char* dot = strrchr(filename, '.');
    if(!dot || dot == filename)
        return NULL;

    return dot + 1;
}

int geode_concat_paths(struct geode_context* ctx, char** dest, size_t* dest_size, const char** parts) {
    if(!dest || !parts)
        return ENOMEM;

    size_t size = 1;
    for(const char** part = parts; *part; part++) {
        size += strlen(*part);
        if(part[1])
            size++;
    }

    char* path = geode_malloc(ctx, size);
    if(!path)
        return ENOMEM;

    *path = '\0';

    for(const char** part = parts; *part; part++) {
        strncat(path, *part, size);
        if(part[1])
            strncat(path, "/", size);
    }

    *dest = path;

    if(dest_size)
        *dest_size = size;

    return 0;
}

