#include <x86_64/cpu/cpu.h>

#include <kernelio.h>
#include <cpuid.h>

#define CPUID_SYSCALL (1 << 11)
#define CPUID_SSE     (1 << 25)
#define EFER_SYSCALL_ENABLE 1

extern void _enable_sse(void);
extern void _enable_x87_fpu(void);
extern void _enable_fxsave(void);

void cpu_enable_features(void) {
    uint32_t eax = 0, ebx = 0, ecx = 0, edx = 0;
    __get_cpuid(0x80000001, &eax, &ebx, &ecx, &edx);

    if((_cpu()->features.syscall_supported = edx & CPUID_SYSCALL)) {
        uint64_t efer = rdmsr(MSR_EFER);
        efer |= EFER_SYSCALL_ENABLE;
        wrmsr(MSR_EFER, efer);
    }

    klog(DEBUG, "syscall support: %hhu", _cpu()->features.syscall_supported);
     
    _cpu()->features.sse_supported = true; // must be supported on x86_64
    _cpu()->features.sse2_supported = true;
    _enable_sse();

    klog(DEBUG, "SSE support: %hhu", _cpu()->features.sse_supported);

    // just assume the presence for now
    _cpu()->features.x87_fpu_supported = true;
    _enable_x87_fpu();
    _enable_fxsave();

    klog(DEBUG, "x87 FPU support: %hhu", _cpu()->features.x87_fpu_supported);
}

