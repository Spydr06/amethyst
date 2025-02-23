#include <unistd.h>

#include <internal/entry.h>

char** __attribute__((section("data"))) environ = NULL;

void __libc_register_environ(char** envp) {
    environ = envp;
}

