#ifndef _AMETHYST_MEM_USER_H
#define _AMETHYST_MEM_USER_H

#include <stddef.h>

int memcpy_from_user(void* kernel_dest, const void* user_src, size_t size);

#endif /* _AMETHYST_MEM_USER_H */

