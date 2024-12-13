#ifndef _GEODE_FS_UTIL_H
#define _GEODE_FS_UTIL_H

#include "context.h"

bool filename_has_file_ext(const char* filename, const char* ext);

const char* get_file_ext(const char* filename);

int geode_concat_paths(struct geode_context* ctx, char** dest, size_t* dest_size, const char** parts);

#endif /* _GEODE_FS_UTIL_H */

