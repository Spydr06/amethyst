#ifndef _ARCH_X86_64_CPU_INTRIN_H
#define _ARCH_X86_64_CPU_INTRIN_H

#include <stdnoreturn.h>
#include <cdefs.h>

static __always_inline void hlt_until_int(void) {
    __asm__ volatile("hlt");
}

static noreturn __always_inline void hlt(void) {
    __asm__ volatile("cli; hlt");
    while(1); // unreachable
}

static __always_inline void rep_nop(void) {
    __asm__ volatile("rep; nop" ::: "memory");
}

static __always_inline uint16_t ds(void)
{
	uint16_t seg;
	__asm__ volatile("movw %%ds,%0" : "=rm" (seg));
	return seg;
}

static __always_inline uint16_t fs(void)
{
	uint16_t seg;
	__asm__ volatile("movw %%fs,%0" : "=rm" (seg));
	return seg;
}

static __always_inline uint16_t gs(void)
{
	uint16_t seg;
	__asm__ volatile("movw %%gs,%0" : "=rm" (seg));
	return seg;
}

static __always_inline void pause(void) {
    __asm__ volatile("pause");
}

#endif /* _ARCH_X86_64_CPU_INTRIN_H */

