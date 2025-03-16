#include "eval.h"
#include "libshard.h"
#include "parse.h"
#include "shell.h"

#include <assert.h>
#include <errno.h>
#include <stdio.h>

int eval_next(struct shell_parser* p, enum shell_eval_flags flags) {
    struct shard_expr* expr = shard_gc_malloc(shell.shard.gc, sizeof(struct shard_expr));
    int err = shell_parse_next(p, expr);
    if(err)
        return err;

    struct shard_lazy_value* lazy = shard_lazy(&shell.shard, expr, &shell.shard.builtin_scope);

    err = shard_eval_lazy(&shell.shard, lazy);
    if(err)
        goto runtime_error;

    if(flags & SH_EVAL_ECHO_RESULTS) {
        struct shard_string result = {0};
        err = shard_value_to_string(&shell.shard, &result, &lazy->eval, 1);
        if(err)
            goto runtime_error;

        fwrite(result.items, sizeof(char), result.count, stdout);
        fputc('\n', stdout);

        shard_string_free(&shell.shard, &result);
    }

    return 0;

runtime_error:
    print_shard_errors();

    return EINVAL;
}


int shell_eval(struct shard_source* source, enum shell_eval_flags flags) {
    struct shell_parser p;
    shell_parser_init(&p, source);

    int err = 0;
    while(!err) {
        err = eval_next(&p, flags);
    }

    shell_parser_free(&p);

    return err == EOF ? 0 : err;
}

