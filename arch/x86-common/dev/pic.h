#ifndef _AMETHYST_X86_COMMON_DEV_PIC_H
#define _AMETHYST_X86_COMMON_DEV_PIC_H

#include <stdint.h>

#define PIC_MASTER_REG 0x20
#define PIC_MASTER_IMR 0x21
#define PIC_SLAVE_REG 0xa0
#define PIC_SLAVE_IMR 0xa1

#define PIC_ICW_1 0x11

void pic_init(void);
void pic_disable(void);
void pic_send_eoi(uint32_t irq);

#endif /* _AMETHYST_X86_COMMON_DEV_PIC_H */

