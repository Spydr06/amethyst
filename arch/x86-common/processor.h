#ifndef _AMETHYST_X86_COMMON_PROCESSOR_H
#define _AMETHYST_X86_COMMON_PROCESSOR_H

#include <cdefs.h>
#include <stdint.h>

_Noreturn static __always_inline void hlt(void) {
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

#endif /* _AMETHYST_X86_COMMON_PROCESSOR_H */

