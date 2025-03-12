#ifndef _SHARD_SHELL_PARSE_H
#define _SHARD_SHELL_PARSE_H

#include <libshard.h>

#include "resource.h"
#include "stmt.h"
#include "token.h"

struct shell_parser {
    struct shell_lexer l;
    struct shell_token token;
};

void shell_parser_init(struct shell_parser* parser, struct shell_resource* resource, struct shard_gc* gc);
void shell_parser_free(struct shell_parser* parser);

int shell_parse_next(struct shell_parser* p, struct shell_stmt* stmt);

#endif /* _SHARD_SHELL_PARSE_H */

