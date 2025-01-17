#ifndef _SYS_MMAN_H
#define _SYS_MMAN_H

#ifdef __cplusplus
extern "C" {
#endif

#include <bits/alltypes.h>

#define MAP_FAILED ((void*) -1)

#define MAP_SHARED  0x01
#define MAP_PRIVATE 0x02
#define MAP_FIXED   0x10
#define MAP_ANON    0x20
#define MAP_ANONYMOUS MAP_ANON

#define PROT_NONE  0
#define PROT_READ  1
#define PROT_WRITE 2
#define PROT_EXEC  4

void *mmap(void* addr, size_t len, int prot, int flags, int fd, off_t offset);
int munmap(void* addr, size_t len);

#ifdef __cplusplus
}
#endif

#endif /* _SYS_MMAN_H */

