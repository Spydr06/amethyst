#ifndef _AMETHYST_X86_64_CPU_MSR_H
#define _AMETHYST_X86_64_CPU_MSR_H

#include <stdint.h>
#include <cdefs.h>

enum msr_register : uint32_t {
    MSR_EFER = 0xC0000080,
    MSR_STAR = 0xC0000081,
    MSR_LSTAR = 0xC0000082,
    MSR_CSTAR = 0xC0000083,
    MSR_FMASK = 0xC0000084,
    MSR_FSBASE = 0xC0000100,
    MSR_GSBASE = 0xC0000101,
    MSR_KERNELGSBASE = 0xC0000102
};

static __always_inline uint64_t rdmsr(enum msr_register which) {
    uint64_t low, high;
    __asm__ volatile(
        "rdmsr"
        : "=a"(low), "=d"(high)
        : "c"(which)
    );
    return (high << 32) | low;
}

static __always_inline void wrmsr(enum msr_register which, uint64_t value) {
    uint32_t low = value & 0xffffffff,
             high = (value >> 32) & 0xffffffff;
    __asm__ volatile(
        "wrmsr"
        :: "a"(low), "d"(high), "c"(which)
    );
}

#endif /* _AMETHYST_X86_64_CPU_MSR_H */

