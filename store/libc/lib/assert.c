#include <assert.h>

#include <stdio.h>
#include <stdlib.h>

_Noreturn void __assert_fail(const char* expr, const char* file, int line, const char* func) {
    fprintf(stderr, "Assertion failed: %s (%s: %s: %d)\n", expr, file, func, line);
    abort();
}

void __here(const char* file, int line, const char* func) {
    fprintf(stderr, "Here (%s: %s: %d)\n", file, func, line);
}

_Noreturn void __unimplemented(const char* file, int line, const char* func) {
    fprintf(stderr, "Unimplemented function (%s: %s: %d)\n", file, func, line);
    abort();
}

_Noreturn void __unreachable(const char* file, int line, const char* func) {
    fprintf(stderr, "Reached unreachable code (%s: %s: %d)\n", file, func, line);
    abort();
}

