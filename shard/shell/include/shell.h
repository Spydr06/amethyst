#ifndef _SHARD_SHELL_H
#define _SHARD_SHELL_H

#include <libshard.h>

#include "resource.h"

#ifndef SHARD_SHELL_VERSION
    #define SHARD_SHELL_VERSION "unknown"
#endif

#ifndef SHELL_HOST_SYSTEM
    #define SHELL_HOST_SYSTEM "unknown"
#endif

#define DEFAULT_HISTORY_SIZE 100

struct shell {
    const char* progname;

    char* prompt;

    uint32_t history_size;
    bool exit_on_error      : 1;
    bool verbose_output     : 1;
    bool repl_should_close  : 1;
};

extern struct shell shell;

void shell_load_defaults(void);

int shell_repl(void);
__attribute__((format(printf, 1, 2))) void errorf(const char *fmt, ...);

struct shell_state {
    struct shard_context shard_context;
    struct shell_parser* parser;

    bool errored;
};

void shell_state_init(struct shell_state* state);

void shell_state_free(struct shell_state* state);

#endif /* _SHARD_SHELL_H */

