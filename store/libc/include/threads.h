#ifndef _THREADS_H
#define _THREADS_H

#if defined(__STDC_VERSION__) && __STDC_VERSION__ < 202300L
    #define thread_local _Thread_local
#else
    #define thread_local thread_local
#endif

#endif /* _THREADS_H */

