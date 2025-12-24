#ifndef _AMETHYST_UTSNAME_H
#define _AMETHYST_UTSNAME_H

#define __UTS_LEN 64

struct utsname {
    char sysname[__UTS_LEN + 1];
    char nodename[__UTS_LEN + 1];
    char release[__UTS_LEN + 1];
    char version[__UTS_LEN + 1];
    char machine[__UTS_LEN + 1];
};

#endif /* _AMETHYST_UTSNAME_H */

