#ifndef _AMETHYST_SYS_FD_H
#define _AMETHYST_SYS_FD_H

struct fd {
    struct file* file;
    int flags;
};

#endif /* _AMETHYST_SYS_FD_H */

