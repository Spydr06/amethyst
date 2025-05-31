#include <libgen.h>

#include <string.h>

// FIXME: make spec compliant
//
char *dirname(char *path) {
    char *base;
    do {
        base = strrchr(path, '/');
    } while(base && base[1] == '\0');

    if(!base)
        return ".";

    base[1] = '\0';
    return path;
}

char *basename(char *path) {
    char *base;

    do {
        base = strrchr(path, '/');
    } while(base && base[1] == '\0');

    return base ? base : path;
}

