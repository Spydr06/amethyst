#ifndef _SHARD_SHELL_PARSE_H
#define _SHARD_SHELL_PARSE_H

#include <libshard.h>

#include "token.h"

struct shell_parser {
    struct shell_state* state;
    struct shell_lexer l;
    struct shell_token token;
};

void shell_parser_init(struct shell_parser* parser, struct shard_source* source);
void shell_parser_free(struct shell_parser* parser);

int shell_parse_next(struct shell_parser* p, struct shard_expr* expr);

#endif /* _SHARD_SHELL_PARSE_H */

