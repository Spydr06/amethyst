#include "shell.h"

#include <libshard.h>
#include <libshard-util.h>

#include <assert.h>
#include <errno.h>
#include <memory.h>
#include <stdlib.h>

#define NULL_VAL() ((struct shard_value) { .type = SHARD_VAL_NULL })

extern char shell_prelude_source[];
extern size_t shell_prelude_source_length;

int shell_call_command(volatile struct shard_evaluator* e, const char* cmdname, struct shard_list* args, struct shard_value* return_val, struct shard_location loc) {
    struct shard_lazy_value* command = NULL;
    int err = shard_set_get(shell.commands, shard_get_ident(&shell.shard, cmdname), &command);
    if(err)
        return ENOENT;

    struct shard_value command_val = shard_eval_lazy2(e, command);
    if(!(command_val.type & SHARD_VAL_CALLABLE))
        return EINVAL;

    struct shard_value arg_list = (struct shard_value) {
        .type = SHARD_VAL_LIST,
        .list.head = args
    };

    *return_val = shard_eval_call(e, command_val, shard_unlazy(&shell.shard, arg_list), loc);
    return 0;
}

int shell_prelude(void) {
    struct shard_source prelude_source;
    int err = shard_string_source(&shell.shard, &prelude_source, "prelude.shard", shell_prelude_source, shell_prelude_source_length, 0);
    if(err)
        return err;

    struct shard_open_source* open_source = malloc(sizeof(struct shard_open_source));
    memset(open_source, 0, sizeof(struct shard_open_source));
    open_source->source = prelude_source;
    open_source->opened = true;

    open_source->auto_close = true;
    open_source->auto_free = true;
    shard_register_open(&shell.shard, "prelude.shard", false, open_source);

    int num_err = shard_eval(&shell.shard, open_source);
    print_shard_errors();
    if(num_err)
        return EINVAL;

    struct shard_value prelude_result = open_source->result;
    if(prelude_result.type != SHARD_VAL_SET) {
        errorf("expected `prelude.shard` to yield value of type `set`, got `%s` instead.", shard_value_type_to_string(&shell.shard, prelude_result.type));
        return EINVAL;
    }

    shard_gc_make_static(shell.shard.gc, prelude_result.set);
    shell.commands = prelude_result.set;

    return 0;
}

