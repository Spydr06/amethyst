#include "token.h"
#include "shell.h"

#include <assert.h>
#include <ctype.h>
#include <string.h>

#include <libshard.h>

static char peek_char(struct shell_lexer* l) {
    if(l->current_loc.offset + 1 >= l->buffer.count)
        return EOF;

    char c = l->buffer.items[l->current_loc.offset];
    return c ? c : EOF;
}

static char next_char(struct shell_lexer* l) {
    if(l->current_loc.offset + 1 >= l->buffer.count)
        return EOF;

    char c = l->buffer.items[l->current_loc.offset];
    
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

static struct shard_location default_location(struct shard_source* source) {
    return (struct shard_location) {
        .src = source,
        .column = 1,
        .offset = 0,
        .line = source->line_offset,
        .width = 1
    };
}

static inline void skip_whitespace(struct shell_lexer* l) {
    while(isspace(peek_char(l)))
        next_char(l);
}

static inline void current_location(struct shell_lexer* l, struct shard_location* loc) {
    memcpy(loc, &l->current_loc, sizeof(struct shard_location));
}

static inline bool is_var_char(char c) {
    return isalnum(c) || c == '-' || c == '_' || c == '.';
}

static inline bool is_special_symbol(char c) {
    return strchr(SHELL_SYMBOLS, c) != NULL;
}

static inline bool is_text_char(char c) {
    return isgraph(c) && !is_special_symbol(c);
}

void lex_init(struct shell_lexer* l, struct shard_source* source) {
    l->source = source;
    l->current_loc = default_location(source);

    source->read_all(source, &l->buffer);
}

void lex_free(struct shell_lexer* l) {
    if(l->source->buffer_dtor)
        l->source->buffer_dtor(&l->buffer);
}

static void escape_code(struct shell_lexer* l, struct shard_string* string) {
    char c = next_char(l);
    switch(c) {
        case 't':
            shard_gc_string_push(shell.shard.gc, string, '\t');
            break;
        case 'v':
            shard_gc_string_push(shell.shard.gc, string, '\v');
            break;
        case '0':
            shard_gc_string_push(shell.shard.gc, string, '\0');
            break;
        case 'n':
            shard_gc_string_push(shell.shard.gc, string, '\n');
            break;
        default:
            if(is_special_symbol(c)) {
                shard_gc_string_push(shell.shard.gc, string, c);
                break;
            }

            shard_gc_string_push(shell.shard.gc, string, '\\');
            shard_gc_string_push(shell.shard.gc, string, c);
    }
}

static void lex_string(struct shell_lexer* l, struct shell_token* token) {
    char quote = next_char(l);

    token->value.count = 0;

    char c;
    while((c = next_char(l)) != quote && c != EOF && c != '\n') {
        if(c == '\\') {
            escape_code(l, &token->value);
            continue;
        }

        shard_gc_string_push(shell.shard.gc, &token->value, c);
    }

    if(c != quote) {
        token->type = SH_TOK_ERROR;
        shard_gc_string_append(shell.shard.gc, &token->value, "unterminated string literal");
        return;
    }

    token->type = SH_TOK_TEXT;
}

static void lex_var(struct shell_lexer* l, struct shell_token* token) {
    token->value.count = 0;

    while(is_var_char(peek_char(l))) {
        char c = next_char(l);
        shard_gc_string_push(shell.shard.gc, &token->value, c);
    }

    token->type = SH_TOK_VAR;
}

static void lex_text(struct shell_lexer* l, struct shell_token* token) {
    token->value.count = 0;

    while(is_text_char(peek_char(l))) {
        char c = next_char(l);
        if(c == '\\') {
            escape_code(l, &token->value);
            continue;
        }

        shard_gc_string_push(shell.shard.gc, &token->value, c);
    }

    token->type = SH_TOK_TEXT;
}

void lex_next(struct shell_lexer* l, struct shell_token* token) {
    memset(token, 0, sizeof(struct shard_token));
repeat_skip_ws:
    skip_whitespace(l); 

    current_location(l, &token->loc);
repeat_switch:

    char c;
    switch(c = peek_char(l)) {
        case '\\':
            next_char(l);
            token->escaped = true;
            goto repeat_switch;
        case '#': {
            while((c = next_char(l)) != EOF && c != '\n');
            goto repeat_skip_ws;
        }
        case '(':
        case ')':
        case '{':
        case '}':
        case ':':
        case ';':
        case '`':
        case '<':
            next_char(l);
            token->type = (enum shell_token_type) c;
            break;
        case '|':
            next_char(l);
            if(peek_char(l) == '|') {
                next_char(l);
                token->type = SH_TOK_OR;
                break;
            }
            
            token->type = SH_TOK_PIPE;

            while(is_var_char(peek_char(l)))
                shard_gc_string_push(shell.shard.gc, &token->value, next_char(l));

            break;
        case '>':
            next_char(l);
            if(peek_char(l) == '>') {
                next_char(l);
                token->type = SH_TOK_REDIRECT_APPEND;
            }
            else
                token->type = SH_TOK_REDIRECT;

            while(is_var_char(peek_char(l)))
                shard_gc_string_push(shell.shard.gc, &token->value, next_char(l));

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
        case '\0':
            token->type = SH_TOK_EOF;
            break;
        default:
            if(is_text_char(c)) {
                lex_text(l, token);
                break;
            }

            token->type = SH_TOK_ERROR;
            shard_gc_string_append(shell.shard.gc, &token->value, "unexpected character");
            break;
    }
}

void shell_token_to_string(struct shard_context* ctx, struct shell_token* token, struct shard_string* dst) {
    if(token->type >= SHELL_SYM1_START && token->type <= SHELL_SYM1_END) {
        shard_string_push(ctx, dst, (char) token->type);
        return;
    }

    if(token->type >= SHELL_SYM2_START && token->type <= SHELL_SYM2_END) {
        shard_string_push(ctx, dst, (char) (token->type >> 8));
        shard_string_push(ctx, dst, (char) token->type);
        return;
    }
    
    switch(token->type) {
        case SH_TOK_EOF:
            shard_string_append(ctx, dst, "<end of file>");
            break;
        case SH_TOK_VAR:
            shard_string_push(ctx, dst, '$');
            // fall through
        case SH_TOK_TEXT:
            shard_string_appendn(ctx, dst, token->value.items, token->value.count);
            break;
        case SH_TOK_ERROR:
            shard_string_append(ctx, dst, "<error \"");
            shard_string_appendn(ctx, dst, token->value.items, token->value.count);
            shard_string_push(ctx, dst, '>');
            break;
        default:
            shard_string_append(ctx, dst, "<unknown token>");
    }
}

