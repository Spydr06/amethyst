#ifndef _SHARD_SHELL_H
#define _SHARD_SHELL_H

#include <libshard.h>
#include <histedit.h>

#ifndef SHARD_SHELL_VERSION
    #define SHARD_SHELL_VERSION "unknown"
#endif

#ifndef SHELL_HOST_SYSTEM
    #define SHELL_HOST_SYSTEM "unknown"
#endif

#define DEFAULT_HISTORY_SIZE 100

#define DEFAULT_PRELUDE_FILE "/etc/shell/prelude.shard"

enum shell_iostream {
    SHELL_STDIN  = 0x01,
    SHELL_STDOUT = 0x02,
    SHELL_STDERR = 0x04
};

struct shell_repl {
    EditLine* el;
    History* history;

    uintmax_t current_line_no;
    struct shard_string current_line;    

    struct shard_source source;
};

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

    struct shell_repl repl;
    struct shard_set* commands;

    struct shard_hashmap aliases;
    struct shard_string_list dir_stack; // pushd, popd
    
    pid_t fg_pid;

    bool jmp_on_sigint;
    jmp_buf sigint_jmp;
};

typedef int (*shell_command_t)(int argc, char** argv);

extern struct shell shell;

void shell_load_defaults(int argc, char** argv);
int shell_load_builtins(void);
int shell_prelude(void);

int shell_repl(void);

int shell_pushd(const char* path);
int shell_popd(void);

__attribute__((format(printf, 1, 2))) void errorf(const char *fmt, ...);
int print_shard_errors(void);

int shell_call_command(volatile struct shard_evaluator* e, const char* cmdname, struct shard_list* args, struct shard_value* return_val, struct shard_location loc);

void install_signal_handlers(void);

int shell_create_process(int argc, char** argv);
int shell_pipe(int64_t src_pid, int64_t dst_pid, enum shell_iostream ios);
int shell_redirect(int64_t src_pid, const char* restrict dst_path, enum shell_iostream ios);

int shell_waitpid(int64_t pid);
int shell_resume(int64_t pid);
int shell_suspend(int64_t pid);

// Shell Builtins

extern struct shard_builtin shell_builtin_createProcess;
extern struct shard_builtin shell_builtin_redirect;
extern struct shard_builtin shell_builtin_pipe;
extern struct shard_builtin shell_builtin_waitProcess;
extern struct shard_builtin shell_builtin_asyncProcess;

extern struct shard_builtin shard_builtin_seq;
extern struct shard_builtin shell_builtin_and;
extern struct shard_builtin shell_builtin_or;

#endif /* _SHARD_SHELL_H */

