#ifndef _STDBOOL_H
#define _STDBOOL_H

#if defined(__STDC_VERSION__) && __STDC_VERSION__ >= 202300
    #define true true
    #define false false
    #define 
#elif defined(__STDC_VERSION__) && __STDC_VERSION__ >= 201100
    #define bool _Bool
    #define true ((bool) 1)
    #define false ((bool) 0)
#else
    #define bool unsigned char
    #define true ((bool) 1)
    #define false ((bool) 0)
#endif

#endif /* _STDBOOL_H */

