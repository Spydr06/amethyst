#ifndef _AMETHYST_SYS_TTY_H
#define _AMETHYST_SYS_TTY_H

#include <cdefs.h>

void tty_init(void);

void early_putchar(int ch);
void early_console_init(void);

#endif /* _AMETHYST_SYS_TTY_H */

