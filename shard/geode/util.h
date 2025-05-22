#ifndef _GEODE_UTIL_H
#define _GEODE_UTIL_H

#include "geode.h"
#include "lifetime.h"

#include <stdbool.h>
#include <unistd.h>

#define MIN(a, b) ((a) < (b) ? (a) : (b))
#define MAX(a, b) ((a) > (b) ? (a) : (b))

bool strendswith(const char *str, const char *suffix);

char *strtrim(char *str);

bool fexists(const char *filepath);
bool fwritable(const char *filepath);
bool freadable(const char *filepath);
bool fexecutable(const char *filepath);

int rmdir_recursive(const char *path);
int mkdir_recursive(const char *path, int mode);

int copy_file(const char *src_path, const char *dst_path);
int copy_file_fd(int fd_in, int fd_out);

int copy_recursive_fd(int fd_in, int fd_out);
int copy_recursive(const char *src_path, const char *dst_path);

ssize_t read_whole_file(lifetime_t *l, const char *path, char **bufptr);

int geode_pushd(struct geode_context *context, const char *path);
int geode_popd(struct geode_context *context);

char *geode_getcwd(lifetime_t *l);

#endif /* _GEODE_UTIL_H */

