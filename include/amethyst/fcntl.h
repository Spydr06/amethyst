#ifndef _AMETHYST_FILE_H
#define _AMETHYST_FILE_H

#undef O_RDONLY
#undef O_WRONLY
#undef O_RDWR
#undef O_CREAT
#undef O_EXCL
#undef O_NOCTTY
#undef O_TRUNC
#undef O_APPEND
#undef O_NONBLOCK
#undef O_DIRECTORY
#undef O_CLOEXEC
#undef O_CLOFORK

enum o_flags {
    O_RDONLY   = 00,
    O_WRONLY   = 01,
    O_RDWR     = 02,

    O_CREAT    = 0100,
    O_EXCL     = 0200,
    O_NOCTTY   = 0400,
    O_TRUNC    = 01000,
    O_APPEND   = 02000,
    O_NONBLOCK = 04000,
    // ...
    O_DIRECTORY = 0100000,
    O_CLOEXEC   = 02000000,
    O_CLOFORK   = 04000000,
    // ...
};

#endif /* _AMETHYST_FILE_H */
