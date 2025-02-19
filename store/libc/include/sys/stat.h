#ifndef _SYS_STAT_H
#define _SYS_STAT_H

#ifdef __cplusplus
extern "C" {
#endif

#include <bits/alltypes.h>
#include <amethyst/stat.h>

int mkdir(const char *pathname, mode_t mode);

int stat(const char *restrict pathname, struct stat *restrict buf);
int fstat(int fd, struct stat *restrict buf);

#ifdef __cplusplus
}
#endif

#endif /* _SYS_STAT_H */

