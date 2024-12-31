#ifndef _INTERNAL_ENTRY_H
#define _INTERNAL_ENTRY_H

struct exec_stack_data {
    int argc;
    char** argv;
    char** envp;
};

void __libc_register_args(int argc, char** argv);

#endif /* _INTERNAL_ENTRY_H */

