#ifndef _SHARD_SHELL_EVAL_H
#define _SHARD_SHELL_EVAL_H

#include <libshard.h>

enum shell_eval_flags {
    SH_EVAL_ECHO_RESULTS  = 0x01,
    SH_EVAL_ECHO_COMMANDS = 0x02,
};

int shell_eval(struct shard_source* source, enum shell_eval_flags flags);

#endif /* _SHARD_SHELL_EVAL_H */

