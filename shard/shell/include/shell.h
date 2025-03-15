#ifndef _SHARD_SHELL_H
#define _SHARD_SHELL_H

#include <libshard.h>

#ifndef SHARD_SHELL_VERSION
    #define SHARD_SHELL_VERSION "unknown"
#endif

#ifndef SHELL_HOST_SYSTEM
    #define SHELL_HOST_SYSTEM "unknown"
#endif

#define DEFAULT_HISTORY_SIZE 100

struct shell {
    const char* progname;
    
    char* progname_copy;
    const char* name;

    char* prompt;

    uint32_t history_size;
    bool exit_on_error      : 1;
    bool verbose_output     : 1;
    bool repl_should_close  : 1;

    struct shard_context shard;
};

extern struct shell shell;

void shell_load_defaults(int argc, char** argv);

int shell_repl(void);
__attribute__((format(printf, 1, 2))) void errorf(const char *fmt, ...);

int shell_load_builtins(void);

enum shell_process_flags {
    SH_PROC_WAIT = 0x01,
};

int shell_process(size_t argc, char** argv, enum shell_process_flags flags);

// Shell Builtins

extern struct shard_builtin shell_builtin_callProgram;

#endif /* _SHARD_SHELL_H */

