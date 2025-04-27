#include <stdlib.h>
#include <string.h>

#include <assert.h>
#include <internal/entry.h>

static int saved_argc;
static char** saved_argv;

void __libc_register_args(int argc, char** argv) {
    saved_argv = argv;
    saved_argc = argc;
}

int getargc(void) {
    return saved_argc;
}

char* getargv(int i) {
    if(i < 0 || i > saved_argc || !saved_argv)
        return NULL;
    return saved_argv[i];
}

char* _getprogname(void) {
    assert(saved_argc > 0);
    return saved_argv[0];
}
