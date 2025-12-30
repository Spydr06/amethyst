#include <unistd.h>
#include <assert.h>

#include <internal/entry.h>

char** environ = NULL;

void __libc_register_environ(char** envp) {
    environ = envp;
}

char *getenv(const char *name) {
    return NULL;
//    assert(0 && "getenv() unimplemented");
}

int setenv(const char *name, const char *value, int overwrite) {
    assert(0 && "setenv() unimplemented");
}

int unsetenv(const char *name) {
    assert(0 && "unsetenv() unimplemented");
}
