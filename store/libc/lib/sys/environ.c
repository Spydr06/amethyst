#include <unistd.h>

#include <internal/entry.h>

char** environ = NULL;

void __libc_register_environ(char** envp) {
    environ = envp;
}

