#ifndef _AMETHYST_X86_64_DEV_CMOS_H
#define _AMETHYST_X86_64_DEV_CMOS_H

#include <stdint.h>
#include <time.h>

enum cmos_port : uint16_t {
    CMOS_ADDRESS = 0x70,
    CMOS_DATA    = 0x71
};

void cmos_init(void);
void cmos_read(struct tm* tm);

#endif /* _AMETHYST_x86_64_DEV_CMOS_H */

