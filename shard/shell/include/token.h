#ifndef _SHARD_SHELL_TOKEN_H
#define _SHARD_SHELL_TOKEN_H

#include "resource.h"

#include <stdio.h>
#include <libshard.h>

#define SHELL_SYMBOLS "{}()$;`&|"

enum shell_token_type {
    SH_TOK_EOF = EOF,
    SH_TOK_ERROR = 0,

    SH_TOK_VAR,
    SH_TOK_TEXT,

    SH_TOK_LBRACE = '{',
    SH_TOK_RBRACE = '}',
    SH_TOK_LPAREN = '(',
    SH_TOK_RPAREN = ')',
    SH_TOK_DOLLAR = '$',
    SH_TOK_SEMICOLON = ';',
    SH_TOK_BACKTICK = '`',
    
    SH_TOK_PIPE = '|',
    SH_TOK_AMPERSAND = '&',

    SH_TOK_AND = '&' + ('&' << 8),
    SH_TOK_OR = '|' + ('|' << 8),
};

struct shell_token {
    enum shell_token_type type;
    struct shell_location loc;
    struct shard_string value;
};

struct shell_lexer {
    struct shell_resource* resource;
    struct shell_location current_loc;
    struct shard_gc* gc;
};

void lex_init(struct shell_lexer* l, struct shell_resource* resource, struct shard_gc* gc);
void lex_free(struct shell_lexer* l);

void lex_next(struct shell_lexer* l, struct shell_token* token);

#endif /* _SHARD_SHELL_TOKEN_H */

