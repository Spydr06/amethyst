#ifndef _AMETHYST_INIT_CMDLINE_H
#define _AMETHYST_INIT_CMDLINE_H

#include <stddef.h>

void cmdline_parse(size_t cmdline_size, const char* cmdline);
const char* cmdline_get(const char* key);

#endif /* _AMETHYST_INIT_CMDLINE_H */

