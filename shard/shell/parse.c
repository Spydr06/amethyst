#include "parse.h"

#include <stdio.h>

void shell_parser_init(struct shell_parser* parser, struct shell_resource* resource, struct shard_gc* gc) {
    memset(parser, 0, sizeof(struct shell_parser));
    lex_init(&parser->l, resource, gc);

    lex_next(&parser->l, &parser->token);
}

void shell_parser_free(struct shell_parser* parser) {
    lex_free(&parser->l);
}

int shell_parse_next(struct shell_parser* p, struct shell_stmt* stmt) {
    return EOF;
}
