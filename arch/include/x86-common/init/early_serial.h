#ifndef _AMETHYST_X86_COMMON_EARLY_SERIAL_H
#define _AMETHYST_X86_COMMON_EARLY_SERIAL_H

#include <stdint.h>

#define X86_EFLAGS_CF_BIT 0 /* carry flag */
#define X86_EFLAGS_CF _BITUL(X86_EFLAGS_CF_BIT)

#define TXR 0 /*  Transmit register (WRITE) */
#define RXR 0 /*  Receive register  (READ)  */
#define IER 1 /*  Interrupt Enable          */
#define IIR 2 /*  Interrupt ID              */
#define FCR 2 /*  FIFO control              */
#define LCR 3 /*  Line control              */
#define MCR 4 /*  Modem control             */
#define LSR 5 /*  Line Status               */
#define MSR 6 /*  Modem Status              */
#define DLL 0 /*  Divisor Latch Low         */
#define DLH 1 /*  Divisor latch High        */

extern int32_t early_serial_base;

#endif /* _AMETHYST_X86_COMMON_EARLY_SERIAL_H */

