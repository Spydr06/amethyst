#ifndef _ARCH_X86_64_ATOMIC_H
#define _ARCH_X86_64_ATOMIC_H

#ifndef PAGESIZE
#define PAGESIZE 4096
#endif

#define a_or a_or
static inline void a_or(volatile int *p, int v){
    __asm__ volatile (
        "lock; or %1, %0"
        : "=m"(*p) : "r"(v) : "memory"
    );
}

#define a_cas a_cas
static inline int a_cas(volatile int *p, int t, int s) {
    __asm__ volatile (
        "lock; cmpxchg %3, %1"
        : "=a"(t), "=m"(*p) : "a"(t), "r"(s) : "memory"
    );
    return t;
}

#endif /* _ARCH_X86_64_ATOMIC_H */

