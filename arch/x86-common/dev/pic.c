#include "pic.h"
#include "../io.h"

void pic_init(void)
{
    // ICW #1
    outb(PIC_MASTER_REG, PIC_ICW_1);
    outb(PIC_SLAVE_REG, PIC_ICW_1);

    // ICW #2 
    outb(PIC_MASTER_IMR, 0x20); // remapping IRQ0-IRQ7 to start from 0x20, i.e 32'th ISR
    outb(PIC_SLAVE_IMR, 0x28); // remapping IRQ8-IRQ15 to start from 0x28, i.e 32'th ISR

    // ICW #3 - MASTER/SLAVE coordination
    outb(PIC_MASTER_IMR, 0x4);
    outb(PIC_SLAVE_IMR, 0x2);

    // ICW #4 - set 80x86 mode
    outb(PIC_MASTER_IMR, 0x01);
    outb(PIC_SLAVE_IMR, 0x01);

    outb(PIC_MASTER_IMR, 0x0);
    outb(PIC_SLAVE_IMR, 0x0);
}

void pic_disable(void)
{
    outb(PIC_MASTER_IMR, 0xff);
    outb(PIC_SLAVE_IMR, 0xff);
}

void pic_send_eoi(uint32_t irq)
{
    if (40 <= irq)
        outb(PIC_SLAVE_REG, 0x20);

    outb(PIC_MASTER_REG, 0x20);
}
