#include "token.h"

#include <assert.h>
#include <ctype.h>

#include <libshard.h>

static char peek_char(struct shell_lexer* l) {
    char c = l->resource->resource[l->current_loc.offset];
    return c ? c : EOF;
}

static char next_char(struct shell_lexer* l) {
    char c = l->resource->resource[l->current_loc.offset];
    
    switch(c) {
        case '\n':
            l->current_loc.line++;
            break;
        case '\0':
        case EOF:
            return EOF;
    }

    l->current_loc.offset++;
    return c;
}

static char prev_char(struct shell_lexer* l) {
    assert(l->current_loc.offset > 0);
    return l->resource->resource[l->current_loc.offset - 1];
}

static struct shell_location default_location(struct shell_resource* resource) {
    return (struct shell_location) {
        .resource = resource,
        .column = 1,
        .offset = 0,
        .line = 1
    };
}

static inline void skip_whitespace(struct shell_lexer* l) {
    while(isspace(peek_char(l)))
        next_char(l);
}

static inline void current_location(struct shell_lexer* l, struct shell_location* loc) {
    memcpy(loc, &l->current_loc, sizeof(struct shell_location));
}

static inline bool is_var_char(char c) {
    return isalnum(c) || c == '-' || c == '_' || c == '.';
}

static inline bool is_text_char(char c) {
    return isgraph(c) && strchr(SHELL_SYMBOLS, c) == NULL;
}

void lex_init(struct shell_lexer* l, struct shell_resource* resource, struct shard_gc* gc) {
    l->resource = resource;
    l->current_loc = default_location(resource);
    l->gc = gc;
}

void lex_free(struct shell_lexer* l) {
}

static void escape_code(struct shell_lexer* l, struct shard_string* string) {
    char c = next_char(l);
    switch(c) {
        case 't':
            shard_gc_string_push(l->gc, string, '\t');
            break;
        case 'v':
            shard_gc_string_push(l->gc, string, '\v');
            break;
        case '0':
            shard_gc_string_push(l->gc, string, '\0');
            break;
        case 'n':
            shard_gc_string_push(l->gc, string, '\n');
            break;
        default:
            shard_gc_string_push(l->gc, string, '\\');
            shard_gc_string_push(l->gc, string, c);
    }
}

static void lex_string(struct shell_lexer* l, struct shell_token* token) {
    char quote = next_char(l);

    struct shard_string string = {0};

    char c;
    while((c = next_char(l)) != quote && c != EOF && c != '\n') {
        if(c == '\\') {
            escape_code(l, &string);
            continue;
        }

        shard_gc_string_push(l->gc, &string, c);
    }

    if(c != quote) {
        token->type = SH_TOK_ERROR;
        shard_gc_string_append(l->gc, &token->value, "unterminated string literal");
        return;
    }

    token->type = SH_TOK_TEXT;
    token->value = string;
}

static void lex_var(struct shell_lexer* l, struct shell_token* token) {
    struct shard_string var_name = {0};

    while(is_var_char(peek_char(l))) {
        char c = next_char(l);
        shard_gc_string_push(l->gc, &var_name, c);
    }

    token->type = SH_TOK_VAR;
    token->value = var_name;
}

static void lex_text(struct shell_lexer* l, struct shell_token* token) {
    struct shard_string text = {0};

    while(is_text_char(peek_char(l))) {
        char c = next_char(l);
        if(c == '\\') {
            escape_code(l, &text);
            continue;
        }

        shard_gc_string_push(l->gc, &text, c);
    }

    token->type = SH_TOK_TEXT;
    token->value = text;
}

void lex_next(struct shell_lexer* l, struct shell_token* token) {
    memset(token, 0, sizeof(struct shard_token));
repeat:
    skip_whitespace(l); 

    current_location(l, &token->loc);

    char c;
    switch(c = peek_char(l)) {
        case '#': {
            while((c = next_char(l)) != EOF && c != '\n');
            goto repeat;
        }
        case '(':
        case ')':
        case '{':
        case '}':
        case ';':
        case '`':
            next_char(l);
            token->type = (enum shell_token_type) c;
            break;
        case '|':
            next_char(l);
            if(peek_char(l) == '|') {
                next_char(l);
                token->type = SH_TOK_OR;
            }
            else
                token->type = SH_TOK_PIPE;
            break;
        case '&':
            next_char(l);
            if(peek_char(l) == '&') {
                next_char(l);
                token->type = SH_TOK_AND;
            }
            else
                token->type = SH_TOK_AMPERSAND;
            break;
        case '\'':
        case '\"':
            lex_string(l, token);
            break;
        case '$':
            next_char(l);
            if(is_var_char(peek_char(l)))
                lex_var(l, token);
            else
                token->type = SH_TOK_DOLLAR;
            break;
        case EOF:
            token->type = SH_TOK_EOF;
            break;
        default:
            if(is_text_char(c)) {
                lex_text(l, token);
                break;
            }

            token->type = SH_TOK_ERROR;
            shard_gc_string_append(l->gc, &token->value, "unexpected character");
            break;
    }
}

