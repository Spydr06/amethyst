#ifndef _AMETHYST_LIBK_ASSERT_H
#define _AMETHYST_LIBK_ASSERT_H

#include <kernelio.h>

#ifdef NO_ASSERT
#define assert(_expr) ((void) 0)
#else
#define assert(expr) ((expr) ? (void) 0 : panic("assertion failed: %s", #expr))
#endif

#endif /* _AMETHYST_LIBK_ASSERT_H */

