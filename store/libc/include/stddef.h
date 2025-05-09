#ifndef _STDDEF_H
#define _STDDEF_H

#include <bits/alltypes.h>

#if __GNUC__ > 3
    #define offsetof(type, member) __builtin_offsetof(type, member)
#else
    #define offsetof(type, member) ((size_t) ((char*) &(((type*) 0)->member) - (char*) 0))
#endif

#endif /* _STDDEF_H */
