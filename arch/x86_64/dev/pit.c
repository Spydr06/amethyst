#include <x86_64/dev/pit.h>
#include <x86_64/dev/io.h>

#include <stdint.h>

uint16_t __pit_divisor = 1;
uint32_t __pit_delta_time = 0;

void init_pit(uint32_t frequency)
{
    uint32_t count, remainder;

    if (frequency <= 18)
        count = 0xffff;
    else if(frequency >= INTERNAL_FREQUENCY)
        count = 1;
    else {
        count = INTERNAL_FREQUENCY_3X / frequency;
        remainder = INTERNAL_FREQUENCY_3X % frequency;

        if(remainder >= INTERNAL_FREQUENCY_3X / 2)
            count++;
        
        count /= 3;
        remainder = count % 3;

        if(remainder >= 1)
            count++;
    }

    __pit_divisor = count & 0xffff;
    __pit_delta_time = (3685982306ULL * count) >> 10;

    outb(I8253_CONTROL_REG, 0x34);
    outb(I8253_DATA_REG, __pit_divisor & 0xff); // LSB
    outb(I8253_DATA_REG, __pit_divisor >> 8); // MSB
}
