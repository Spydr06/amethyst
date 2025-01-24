#ifndef _ASSERT_H
#define _ASSERT_H

#ifdef NDEBUG
    #define assert(c) ((void) (c))

    #ifdef _AMETHYST_SRC
        #define here() ((void) 0)
        #define unimplemented() ((void) 0)
    #endif
#else
    #define assert(x) ((void)((x) || (__assert_fail(#x, __FILE__, __LINE__, __func__), 0)))

    #ifdef _AMETHYST_SOURCE
        #define here() (__here(__FILE__, __LINE__, __func__))
        #define unimplemented() (__unimplemented(__FILE__, __LINE__, __func__))
        #define unreachable() (__unreachable(__FILE__, __LINE__, __func__))
    #endif
#endif

#ifdef __cplusplus
extern "C" {
#endif

_Noreturn void __assert_fail(const char*, const char*, int, const char*);

#ifdef _AMETHYST_SOURCE
    void __here(const char*, int, const char*);
    _Noreturn void __unimplemented(const char*, int, const char*);
    _Noreturn void __unreachable(const char*, int, const char*);
#endif

#ifdef __cplusplus
}
#endif

#endif /* _ASSERT_H */

