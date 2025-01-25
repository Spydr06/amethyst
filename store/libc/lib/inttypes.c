#include <inttypes.h>

#define _AMETHYST_SOURCE
#include <assert.h>

intmax_t imaxabs(intmax_t j) {
    return j < 0 ? -j : j;
}

imaxdiv_t imaxdiv(intmax_t numer, intmax_t denom) {
    return (imaxdiv_t){ .quot = numer / denom, .rem = numer % denom };
}

intmax_t strtoimax(const char *restrict nptr, char **restrict endptr, int base) {
    unimplemented();
}

uintmax_t strtoumax(const char *restrict nptr, char **restrict endptr, int base) {
    unimplemented();
}

