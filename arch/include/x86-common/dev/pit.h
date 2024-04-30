#ifndef _AMETHYST_X86_COMMON_DEV_PIT_H
#define _AMETHYST_X86_COMMON_DEV_PIT_H

#include <stdint.h>

#define I8253_CONTROL_REG 0x43
#define I8253_DATA_REG  0x40

#define INTERNAL_FREQUENCY 1193182ul
#define INTERNAL_FREQUENCY_3X (INTERNAL_FREQUENCY * 3ul)

#define SYSTEM_TICK_FREQUENCY 1000 /* millis */

extern uint16_t __pit_divisor;
extern uint32_t __pit_delta_time;

void init_pit(uint32_t frequency);

#endif /* _AMETHYST_X86_COMMON_DEV_PIT_H */

