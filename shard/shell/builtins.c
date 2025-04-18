#include "shell.h"

#include <libshard.h>

#define __USE_XOPEN2K

#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define NULL_VAL() ((struct shard_value) { .type = SHARD_VAL_NULL })
#define INT_VAL(i) ((struct shard_value) { .type = SHARD_VAL_INT, .integer = (int64_t)(i) })

#define STRING_VAL(s, l) ((struct shard_value) { .type = SHARD_VAL_STRING, .string = (s), .strlen = (l) })
#define CSTRING_VAL(s) STRING_VAL(s, strlen((s)))

#define SHELL_BUILTIN(name, cname, ...)                                                                                                                 \
        static struct shard_value builtin_##cname(volatile struct shard_evaluator* e, struct shard_builtin* builtin, struct shard_lazy_value** args);   \
        struct shard_builtin shell_builtin_##cname = SHARD_BUILTIN(name, builtin_##cname, __VA_ARGS__)

extern struct shard_builtin shard_builtin_toString;
extern struct shard_builtin shell_builtin_createProcess;
extern struct shard_builtin shell_builtin_redirect;
extern struct shard_builtin shell_builtin_pipe;
extern struct shard_builtin shell_builtin_waitProcess;
extern struct shard_builtin shell_builtin_asyncProcess;

SHELL_BUILTIN("shell.createProcess", createProcess, SHARD_VAL_LIST);
SHELL_BUILTIN("shell.waitProcess", waitProcess, SHARD_VAL_INT);
SHELL_BUILTIN("shell.asyncProcess", asyncProcess, SHARD_VAL_INT);

SHELL_BUILTIN("shell.redirect", redirect, SHARD_VAL_INT, SHARD_VAL_STRING, SHARD_VAL_INT);
SHELL_BUILTIN("shell.pipe", pipe, SHARD_VAL_INT, SHARD_VAL_INT, SHARD_VAL_INT);

SHELL_BUILTIN("shell.changeDir", changeDir, SHARD_VAL_STRING | SHARD_VAL_PATH);
SHELL_BUILTIN("shell.exit", exit, SHARD_VAL_INT);
SHELL_BUILTIN("shell.printError", printError, SHARD_VAL_STRING);
SHELL_BUILTIN("shell.printLn", printLn, SHARD_VAL_STRING);
SHELL_BUILTIN("shell.getAlias", getAlias, SHARD_VAL_STRING);
SHELL_BUILTIN("shell.setAlias", setAlias, SHARD_VAL_STRING, SHARD_VAL_STRING);
SHELL_BUILTIN("shell.getEnv", getEnv, SHARD_VAL_STRING);
SHELL_BUILTIN("shell.setEnv", setEnv, SHARD_VAL_STRING, SHARD_VAL_STRING);
SHELL_BUILTIN("shell.and", and, SHARD_VAL_INT, SHARD_VAL_INT);
SHELL_BUILTIN("shell.or", or, SHARD_VAL_INT, SHARD_VAL_INT);

struct shard_builtin* shell_builtins[] = {
    &shell_builtin_createProcess,
    &shell_builtin_waitProcess,
    &shell_builtin_asyncProcess,
    &shell_builtin_redirect,
    &shell_builtin_pipe,

    &shell_builtin_changeDir,
    &shell_builtin_exit,
    &shell_builtin_printError,
    &shell_builtin_printLn,
    &shell_builtin_and,
    &shell_builtin_or,
    &shell_builtin_getAlias,
    &shell_builtin_setAlias,
    &shell_builtin_getEnv,
    &shell_builtin_setEnv
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

// FIXME: support process management for builtin processes aswell
static struct shard_value builtin_createProcess(volatile struct shard_evaluator* e, struct shard_builtin* builtin, struct shard_lazy_value** args) {
    struct shard_value prog_args = shard_builtin_eval_arg(e, builtin, args, 0);
    assert(prog_args.type == SHARD_VAL_LIST);

    struct shard_list* head = prog_args.list.head;
    assert(head != NULL);

    struct shard_value head_val = shard_eval_lazy2(e, head->value);
    if(head_val.type & SHARD_VAL_TEXTUAL) {
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

    return INT_VAL(shell_create_process(argc, argv));
}

static struct shard_value builtin_waitProcess(volatile struct shard_evaluator* e, struct shard_builtin* builtin, struct shard_lazy_value** args) {
    struct shard_value pid = shard_builtin_eval_arg(e, builtin, args, 0);
    return INT_VAL(shell_waitpid(pid.integer));
}

static struct shard_value builtin_asyncProcess(volatile struct shard_evaluator* e, struct shard_builtin* builtin, struct shard_lazy_value** args) {
    struct shard_value pid = shard_builtin_eval_arg(e, builtin, args, 0);
    return INT_VAL(shell_resume(pid.integer));
}

static struct shard_value builtin_redirect(volatile struct shard_evaluator* e, struct shard_builtin* builtin, struct shard_lazy_value** args) {
    struct shard_value streams = shard_builtin_eval_arg(e, builtin, args, 0);
    struct shard_value dest = shard_builtin_eval_arg(e, builtin, args, 1);
    struct shard_value source = shard_builtin_eval_arg(e, builtin, args, 2);

    int err = shell_pipe(source.integer, dest.integer, streams.integer);
    if(err)
        shard_eval_throw(e, e->error_scope->loc, "failed creating pipe: %s", strerror(err));

    return INT_VAL(source.integer);
}

static struct shard_value builtin_pipe(volatile struct shard_evaluator* e, struct shard_builtin* builtin, struct shard_lazy_value** args) {
    struct shard_value streams = shard_builtin_eval_arg(e, builtin, args, 0);
    struct shard_value dest = shard_builtin_eval_arg(e, builtin, args, 1);
    struct shard_value source = shard_builtin_eval_arg(e, builtin, args, 2);

    int err = shell_redirect(source.integer, dest.string, streams.integer);
    if(err)
        shard_eval_throw(e, e->error_scope->loc, "failed creating redirection: %s", strerror(err));

    return INT_VAL(source.integer);
}

/*static struct shard_value builtin_callProgram(volatile struct shard_evaluator* e, struct shard_builtin* builtin, struct shard_lazy_value** args) {
}*/

static struct shard_value builtin_changeDir(volatile struct shard_evaluator* e, struct shard_builtin* builtin, struct shard_lazy_value** args) {
    struct shard_value arg = shard_builtin_eval_arg(e, builtin, args, 0);
    const char* dir = arg.type == SHARD_VAL_STRING ? arg.string : arg.path;

    int err = chdir(dir);
    if(err < 0)
        return INT_VAL(errno);
    return INT_VAL(0);
}

static struct shard_value builtin_exit(volatile struct shard_evaluator* e, struct shard_builtin* builtin, struct shard_lazy_value** args) {
    struct shard_value exit_code = shard_builtin_eval_arg(e, builtin, args, 0);
    exit(exit_code.integer);
}

static struct shard_value builtin_printError(volatile struct shard_evaluator* e, struct shard_builtin* builtin, struct shard_lazy_value** args) {
    struct shard_value message = shard_builtin_eval_arg(e, builtin, args, 0);
    struct shard_location loc = e->error_scope->loc;

    errorf("%s (%s:%u:%u)", message.string, loc.src->origin, loc.line, loc.column);
    
    return INT_VAL(EXIT_FAILURE);
}

static struct shard_value builtin_printLn(volatile struct shard_evaluator* e, struct shard_builtin* builtin, struct shard_lazy_value** args) {
    struct shard_value message = shard_builtin_eval_arg(e, builtin, args, 0);
    
    printf("%s\n", message.string);

    return INT_VAL(0);
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

static struct shard_value builtin_getAlias(volatile struct shard_evaluator* e, struct shard_builtin* builtin, struct shard_lazy_value** args) {
    struct shard_value name = shard_builtin_eval_arg(e, builtin, args, 0);

    void* value = shard_hashmap_get(&shell.aliases, name.string);
    if(value)
        return CSTRING_VAL(value);
    else
        return NULL_VAL();
}

static struct shard_value builtin_setAlias(volatile struct shard_evaluator* e, struct shard_builtin* builtin, struct shard_lazy_value** args) {
    struct shard_value name = shard_builtin_eval_arg(e, builtin, args, 0);
    struct shard_value value = shard_builtin_eval_arg(e, builtin, args, 1);

    shard_gc_make_static(shell.shard.gc, (void*) value.string);
    int err = shard_hashmap_put(&shell.shard, &shell.aliases, name.string, (char*) value.string);
    return INT_VAL(err);
}

static struct shard_value builtin_getEnv(volatile struct shard_evaluator* e, struct shard_builtin* builtin, struct shard_lazy_value** args) {
    struct shard_value name = shard_builtin_eval_arg(e, builtin, args, 0);

    char* value = getenv(name.string);
    if(!value)
        return NULL_VAL();
    return CSTRING_VAL(value);
}

static struct shard_value builtin_setEnv(volatile struct shard_evaluator* e, struct shard_builtin* builtin, struct shard_lazy_value** args) {
    struct shard_value name = shard_builtin_eval_arg(e, builtin, args, 0);
    struct shard_value value = shard_builtin_eval_arg(e, builtin, args, 1);

    int err = setenv(name.string, value.string, 1);
    if(err < 0)
        return INT_VAL(errno);
    return INT_VAL(0);
}

