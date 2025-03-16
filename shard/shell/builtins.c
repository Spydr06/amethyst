#include "shell.h"

#include <libshard.h>

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define INT_VAL(i) ((struct shard_value) { .type = SHARD_VAL_INT, .integer = (int64_t)(i) })

static struct shard_value builtin_callProgram(volatile struct shard_evaluator* e, struct shard_builtin* builtin, struct shard_lazy_value** args);
static struct shard_value builtin_exit(volatile struct shard_evaluator* e, struct shard_builtin* builtin, struct shard_lazy_value** args);
static struct shard_value builtin_printError(volatile struct shard_evaluator* e, struct shard_builtin* builtin, struct shard_lazy_value** args);
static struct shard_value builtin_and(volatile struct shard_evaluator* e, struct shard_builtin* builtin, struct shard_lazy_value** args);
static struct shard_value builtin_or(volatile struct shard_evaluator* e, struct shard_builtin* builtin, struct shard_lazy_value** args);

extern struct shard_builtin shard_builtin_toString;

struct shard_builtin shell_builtin_callProgram = SHARD_BUILTIN("shell.callProgram", builtin_callProgram, SHARD_VAL_LIST);
struct shard_builtin shell_builtin_exit = SHARD_BUILTIN("shell.exit", builtin_exit, SHARD_VAL_INT);
struct shard_builtin shell_builtin_printError = SHARD_BUILTIN("shell.printError", builtin_printError, SHARD_VAL_STRING);

struct shard_builtin shell_builtin_and = SHARD_BUILTIN("shell.and", builtin_and, SHARD_VAL_INT, SHARD_VAL_INT);
struct shard_builtin shell_builtin_or = SHARD_BUILTIN("shell.or", builtin_or, SHARD_VAL_INT, SHARD_VAL_INT);

struct shard_builtin* shell_builtins[] = {
    &shell_builtin_callProgram,
    &shell_builtin_exit,
    &shell_builtin_printError,
    &shell_builtin_and,
    &shell_builtin_or,
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

    struct shard_list* head = prog_args.list.head;
    assert(head != NULL);

    struct shard_value head_val = shard_eval_lazy2(e, head->value);
    if(head_val.type == SHARD_VAL_STRING) {
        struct shard_value return_val;
        int err = shell_call_command(e, head_val.string, head->next, &return_val, e->error_scope->loc);
        if(!err) {
            return return_val;
        }
    }
    else if(head_val.type & SHARD_VAL_CALLABLE) {
        while((head = head->next)) {
            head_val = shard_eval_call(e, head_val, head->value, e->error_scope->loc);
        }

        return head_val;
    }

    size_t argc = shard_list_length(prog_args.list.head);
    char** argv = shard_gc_malloc(shell.shard.gc, sizeof(char*) * (argc + 1));

    argv[argc] = NULL;

    for(size_t i = 0; head; i++, head = head->next) {
        struct shard_value arg = shard_builtin_toString.callback(e, &shard_builtin_toString, &head->value); 
        argv[i] = (char*) arg.string;
    }

    return INT_VAL(shell_process(argc, argv, SH_PROC_WAIT));
}

static struct shard_value builtin_exit(volatile struct shard_evaluator* e, struct shard_builtin* builtin, struct shard_lazy_value** args) {
    struct shard_value exit_code = shard_builtin_eval_arg(e, builtin, args, 0);
    exit(exit_code.integer);
}

static struct shard_value builtin_printError(volatile struct shard_evaluator* e, struct shard_builtin* builtin, struct shard_lazy_value** args) {
    struct shard_value message = shard_builtin_eval_arg(e, builtin, args, 0);
    errorf("%s", message.string);
    return INT_VAL(EXIT_FAILURE);
}

static struct shard_value builtin_and(volatile struct shard_evaluator* e, struct shard_builtin* builtin, struct shard_lazy_value** args) {
    struct shard_value lhs = shard_builtin_eval_arg(e, builtin, args, 0);
    if(lhs.integer)
        return INT_VAL(1);

    struct shard_value rhs = shard_builtin_eval_arg(e, builtin, args, 1);
    return INT_VAL(rhs.integer ? 1 : 0);
}

static struct shard_value builtin_or(volatile struct shard_evaluator* e, struct shard_builtin* builtin, struct shard_lazy_value** args) {
    struct shard_value lhs = shard_builtin_eval_arg(e, builtin, args, 0);
    if(!lhs.integer)
        return INT_VAL(0);

    struct shard_value rhs = shard_builtin_eval_arg(e, builtin, args, 1);
    return INT_VAL(rhs.integer ? 1 : 0);
}

