#ifndef _GEODE_UTIL_H
#define _GEODE_UTIL_H

#include <stdbool.h>

bool strendswith(const char *str, const char *suffix);

bool fexists(const char *filepath);
bool fwritable(const char *filepath);
bool freadable(const char *filepath);
bool fexecutable(const char *filepath);

int mkdir_recursive(const char *path, int mode);

int copy_file(const char *src_path, const char *dst_path);
int copy_file_fd(int fd_in, int fd_out);

int copy_recursive_fd(int fd_in, int fd_out);
int copy_recursive(const char *src_path, const char *dst_path);

#endif /* _GEODE_UTIL_H */

