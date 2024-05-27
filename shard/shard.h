#ifndef _SHARD_H
#define _SHARD_H

#include <libshard.h>

#define EITHER(a, b) ((a) ? (a) : (b))
#define MAX(a, b) ((a) > (b) ? (a) : (b))
#define LEN(arr) (sizeof((arr)) / sizeof((arr)[0]))

#define C_RED "\033[31m"
#define C_BLACK "\033[90m"
#define C_BLUE "\033[34m"
#define C_PURPLE "\033[35m"

#define C_BLD "\033[1m"
#define C_RST "\033[0m"
#define C_NOBLD "\033[22m"

int shard_repl(struct shard_context* ctx, bool echo_result);

#endif /* _SHARD_H  */
