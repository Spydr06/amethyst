#include "shell.h"

#include <libshard.h>

#include <assert.h>
#include <string.h>
#include <stdio.h>

static struct shard_value builtin_callProgram(volatile struct shard_evaluator* e, struct shard_builtin* builtin, struct shard_lazy_value** args);

extern struct shard_builtin shard_builtin_toString;

struct shard_builtin shell_builtin_callProgram = SHARD_BUILTIN("shell.callProgram", builtin_callProgram, SHARD_VAL_LIST);

struct shard_builtin* shell_builtins[] = {
    &shell_builtin_callProgram
};

int shell_load_builtins(void) {
    int err;
    for(size_t i = 0; i < sizeof(shell_builtins) / sizeof(void*); i++) {
        err = shard_define_builtin(&shell.shard, shell_builtins[i]);
        if(err) {
            errorf("failed defining builtin `%s`: %s", shell_builtins[i]->full_ident, strerror(err));
            return err;
        }
    }

    return 0;
}

static struct shard_value builtin_callProgram(volatile struct shard_evaluator* e, struct shard_builtin* builtin, struct shard_lazy_value** args) {
    struct shard_value prog_args = shard_builtin_eval_arg(e, builtin, args, 0);
    assert(prog_args.type == SHARD_VAL_LIST);

    size_t argc = shard_list_length(prog_args.list.head);
    char** argv = shard_gc_malloc(shell.shard.gc, sizeof(char*) * (argc + 1));

    argv[argc] = NULL;

    struct shard_list* head = prog_args.list.head;
    for(size_t i = 0; head; i++, head = head->next) {
        struct shard_value arg = shard_builtin_toString.callback(e, &shard_builtin_toString, &head->value); 
        argv[i] = (char*) arg.string;
    }

    return (struct shard_value) {
        .type = SHARD_VAL_INT,
        .integer = shell_process(argc, argv, SH_PROC_WAIT)
    };
}

