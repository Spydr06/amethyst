#ifndef _SHARD_SHELL_TOKEN_H
#define _SHARD_SHELL_TOKEN_H

#include <stdio.h>
#include <libshard.h>

#define SHELL_SYMBOLS "{}()$;:\"'`&|<>"

#define SHELL_SYM1_START    10
#define SHELL_SYM1_END      UINT8_MAX
#define SHELL_SYM2_START    ((int) UINT8_MAX + 1)
#define SHELL_SYM2_END      UINT16_MAX

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

    SH_TOK_REDIRECT = '>',
    SH_TOK_REDIRECT_APPEND = '>' + ('>' << 8),

    SH_TOK_PIPE = '|',
    SH_TOK_AMPERSAND = '&',

    SH_TOK_AND = '&' + ('&' << 8),
    SH_TOK_OR = '|' + ('|' << 8),
};

struct shell_token {
    enum shell_token_type type;
    struct shard_location loc;
    struct shard_string value;
    bool escaped;
};

struct shell_lexer {
    struct shard_source* source;
    struct shard_string buffer;
    struct shard_location current_loc;
};

void lex_init(struct shell_lexer* l, struct shard_source* source);
void lex_free(struct shell_lexer* l);

void lex_next(struct shell_lexer* l, struct shell_token* token);

void shell_token_to_string(struct shard_context* ctx, struct shell_token* token, struct shard_string* dest);

#endif /* _SHARD_SHELL_TOKEN_H */

