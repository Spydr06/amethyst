#ifndef _AMETHYST_MEM_USER_H
#define _AMETHYST_MEM_USER_H

#include <stddef.h>
#include <string.h>

int memcpy_from_user(void* kernel_dest, const void* user_src, size_t size);

int user_strlen(const char* str, size_t* size);

// implemented in mmu.c
bool is_userspace_addr(const void* a);

static inline int memcpy_maybe_from_user(void* kernel, const void* user, size_t size) {
    if(is_userspace_addr(user))
        return memcpy_from_user(kernel, user, size);

    memcpy(kernel, user, size);
    return 0;
}

#endif /* _AMETHYST_MEM_USER_H */

