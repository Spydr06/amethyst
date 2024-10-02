#ifndef _GEODE_H
#define _GEODE_H

#include <libshard.h>
#include <stdio.h>

#define EITHER(a, b) ((a) ? (a) : (b))
#define MAX(a, b) ((a) > (b) ? (a) : (b))
#define LEN(arr) (sizeof((arr)) / sizeof((arr)[0]))

#define TUPLE(...) { __VA_ARGS__ }

#define C_RED "\033[31m"
#define C_BLACK "\033[90m"
#define C_BLUE "\033[34m"
#define C_PURPLE "\033[35m"

#define C_BLD "\033[1m"
#define C_RST "\033[0m"
#define C_NOBLD "\033[22m"

#define errorf(fmt, ...) (fprintf(stderr, C_RED C_BLD "[error]" C_NOBLD " " fmt C_RST __VA_OPT__(,) __VA_ARGS__))

#define infof(ctx, ...) ((ctx)->flags.verbose ? fprintf(stdout, C_BLD "[info]" C_NOBLD " " __VA_ARGS__) : 0)

#ifdef unreachable
    #undef unreachable
#endif

#define unreachable() do {                                                                                         \
        errorf("%s:%d:%s `unreachable()` encountered. This should never happen.\n", __FILE__, __LINE__, __func__); \
        exit(EXIT_FAILURE);                                                                                        \
    } while(0)

struct geode_context;

void geode_generate_initrd(struct geode_context* ctx, const char* path);

#endif /* _GEODE_H */

