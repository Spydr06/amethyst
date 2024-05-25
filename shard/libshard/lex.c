#define _LIBSHARD_INTERNAL
#include <libshard.h>

#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>
#include <stddef.h>

#define PATH_MAX 4096

#define LOC_FROM_SOURCE(src, w) (           \
    (struct shard_location) {               \
        .src = (src),                       \
        .line = ((unsigned) (src)->line),   \
        .width = (w),                       \
        .offset = (src)->tell((src)) - (w)  \
    })

#define BASIC_TOK(tok, src, type) (                 \
    init_token((tok), (type),                       \
        LOC_FROM_SOURCE(src, token_widths[(type)]), \
        (union shard_token_value){.string = NULL}   \
    ))

#define STRING_TOK(tok, src, type, str, loc) (      \
    init_token((tok), (type),                       \
        (loc),                                      \
        (union shard_token_value){.string = (str)}  \
    ))

#define ERR_TOK(tok, src, err) (                    \
    init_token((tok), SHARD_TOK_ERR,                \
        LOC_FROM_SOURCE(src, 1),                    \
        (union shard_token_value){.string = (err)}  \
    ))

#define KEYWORD_TOK(tok, src, kind) BASIC_TOK(tok, src, SHARD_TOK_##kind)
#define EOF_TOK(tok, kind) KEYWORD_TOK(tok, src, EOF);

enum path_mode {
    PATH_NONE,
    PATH_INC,
    PATH_REL,
    PATH_ABS,
    PATH_HOME,
};

static char tmpbuf[PATH_MAX + 1];

static const unsigned token_widths[] = {
    0,
    0,
    0,
    0,
    0,
    0,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    2,
    2,
    1,
    2,
    1,
    2,
    1,
    1,
    1,
    3,
    2,
    2,
    1,
    1,
    1,
    1,
    1,
    1,
    1,
    2,
    2,
    2,
    3,
    4,
    5,
    3,
    2,
    2,
    4,
    4,
    6,
    3,
    2,
    4
};

static_assert((sizeof(token_widths) / sizeof(unsigned)) == _SHARD_TOK_LEN);

static const struct {
    const char* keyword;
    enum shard_token_type type;
} keywords[] = {
    {"null", SHARD_TOK_NULL},
    {"true", SHARD_TOK_TRUE},
    {"false", SHARD_TOK_FALSE},
    {"rec", SHARD_TOK_REC},
    {"or", SHARD_TOK_OR},
    {"if", SHARD_TOK_IF},
    {"then", SHARD_TOK_THEN},
    {"else", SHARD_TOK_ELSE},
    {"assert", SHARD_TOK_ASSERT},
    {"let", SHARD_TOK_LET},
    {"in", SHARD_TOK_IN},
    {"with", SHARD_TOK_WITH},
    {NULL, SHARD_TOK_EOF},
};

static enum shard_token_type get_keyword(const char* word) {
    for(unsigned i = 0; keywords[i].keyword; i++) {
        if(strcmp(keywords[i].keyword, word) == 0)
            return keywords[i].type;
    }

    return SHARD_TOK_IDENT;
}

static inline void init_token(struct shard_token* tok, enum shard_token_type type, struct shard_location loc, union shard_token_value value) {
    tok->type = type;
    tok->location = loc;
    tok->value = value;
}

static inline bool is_ident_char(char c) {
    return isalnum(c) || c == '_' || c == '-';
}

static int lex_ident(struct shard_context* ctx, struct shard_source* src, struct shard_token* token) {
    unsigned start = src->tell(src);

    int c, i = 0;
    while(is_ident_char(c = src->getc(src))) {
        if(i >= (int) LEN(tmpbuf)) {
            ERR_TOK(token, src, "identifier too long");
            return ENAMETOOLONG;
        }

        tmpbuf[i++] = (char) c;
    }

    unsigned end = src->tell(src);
    
    tmpbuf[i] = '\0';
    src->ungetc(c, src);

    enum shard_token_type type = get_keyword(tmpbuf);
    if(type == SHARD_TOK_IDENT) {
        char* word = shard_arena_malloc(ctx, ctx->idents, end - start);
        memcpy(word, tmpbuf, end - start);

        struct shard_location loc = {
            .src = src,
            .line = src->line,
            .offset = start,
            .width = end - start - 1
        };

        init_token(token, type, loc, (union shard_token_value){.string = word});
    }
    else
        BASIC_TOK(token, src, type);
    return 0;
}

static int lex_number(struct shard_context* ctx __attribute__((unused)), struct shard_source* src, struct shard_token* token) {
    unsigned start = src->tell(src);

    bool floating = false;
    int c, i = 0;
    while(isdigit(c = src->getc(src)) || c == '\'' || c == '.') {
        if(i >= (int) LEN(tmpbuf)) {
            ERR_TOK(token, src, "floating literal too long");
            return ENAMETOOLONG;
        }

        switch(c) {
            case '\'':
                break;
            case '.':
                if(floating) {
                    ERR_TOK(token, src, "more than one period (`.`) in floating literal");
                    return EINVAL;
                }
                floating = true;
                /* fall through */
            default:
                tmpbuf[i++] = (char) c;
        }
    }

    tmpbuf[i] = '\0';
    src->ungetc(c, src);
    
    unsigned end = src->tell(src);

    struct shard_location loc = {
        .src = src,
        .line = src->line,
        .offset = start,
        .width = end - start
    };

    errno = 0;
    if(floating) {
        double d = strtod(tmpbuf, NULL);
        if(errno) {
            ERR_TOK(token, src, strerror(errno));
            return errno;
        }
        init_token(token, SHARD_TOK_FLOAT, loc, (union shard_token_value){.floating = d});
    }
    else {
        int64_t i = (int64_t) strtoull(tmpbuf, NULL, 10);
        if(errno) {
            ERR_TOK(token, src, strerror(errno));
            return errno;
        }
        init_token(token, SHARD_TOK_INT, loc, (union shard_token_value){.integer = i});
    }
    return 0;
}

static int is_double_quote(int c) {
    return c == '"';
}

static int is_gt_sign(int c) {
    return c == '>';
}

static int lex_string(struct shard_context* ctx, struct shard_source* src, struct shard_token* token, int (*is_end_quote)(int), enum path_mode pmode) {
    unsigned start = src->tell(src) - 1;
    
    struct shard_string str = {0};

    switch(pmode) {
        case PATH_NONE:
        case PATH_ABS:
            break;
        case PATH_REL: {
            if(!ctx->realpath || !ctx->dirname || !src->origin) {
                ERR_TOK(token, src, "relative paths are disabled");
                return EINVAL;
            }

            char* current_path = ctx->malloc(strlen(src->origin) + 1);
            strcpy(current_path, src->origin);
            ctx->dirname(current_path);

            if(!ctx->realpath(current_path, tmpbuf)) {
                ERR_TOK(token, src, "could not resolve relative path");
                return errno;
            }

            dynarr_append_many(ctx, &str, tmpbuf, strlen(tmpbuf));
            dynarr_append(ctx, &str, '/');
            ctx->free(current_path);
        } break;
        case PATH_INC:
            break;
        case PATH_HOME:
            if(!ctx->home_dir) {
                ERR_TOK(token, src, "no home directory specified; Home paths are disabled");
                return EINVAL;
            }

            dynarr_append_many(ctx, &str, ctx->home_dir, strlen(ctx->home_dir));
            break;
    }

    bool escaped = false;
    int c;
    while(!is_end_quote(c = src->getc(src)) || escaped) {
        dynarr_append(ctx, &str, c);

        escaped = false;
        switch(c) {
        case '\\':
            escaped = true;
            break;
        case '\n':
            src->line++;
            ERR_TOK(token, src, "unterminated string literal before newline");
            break;
        case EOF:
            dynarr_free(ctx, &str);
            ERR_TOK(token, src, "unterminated string literal");
            return EINVAL;
        }
    }

    dynarr_append(ctx, &str, '\0');

    dynarr_append(ctx, &ctx->string_literals, str.items);

    unsigned end = src->tell(src);
    struct shard_location loc = {
        .src = src,
        .line = src->line,
        .offset = start - 1,
        .width = end - start + 1
    };

    STRING_TOK(token, src, pmode == PATH_NONE ? SHARD_TOK_STRING : SHARD_TOK_PATH, str.items, loc);
    return 0;
}

static int lex_multiline_string(struct shard_context* ctx, struct shard_source* src, struct shard_token* token) {
    unsigned start = src->tell(src) - 1;

    struct shard_string str = {0};

    bool escaped[3] = {0};
    int c[2] = {0, 0};
    for(;;) {
        c[1] = c[0];
        c[0] = src->getc(src);

        escaped[2] = escaped[1];
        escaped[1] = escaped[0];
        escaped[0] = false;
        
        switch(c[0]) {
            case '\'':
                if(escaped[1] || escaped[2])
                    dynarr_append(ctx, &str, '\'');
                else if(c[1] == '\'')
                    goto break_loop;
                break;
            case '\n':
                dynarr_append(ctx, &str, '\n');
                break;
            case '\\':
                escaped[0] = true;
                dynarr_append(ctx, &str, '\\');
                break;
            case EOF:
                dynarr_free(ctx, &str);
                ERR_TOK(token, src, "unterminated multiline string literal, expect `''`");
                return EINVAL;
            default:
                dynarr_append(ctx, &str, c[0]);
        }
    }

break_loop:

    dynarr_append(ctx, &str, '\0');
    dynarr_append(ctx, &ctx->string_literals, str.items);

    unsigned end = src->tell(src);
    struct shard_location loc = {
        .src = src,
        .line = src->line,
        .offset = start - 1,
        .width = end - start + 1
    };

    STRING_TOK(token, src, SHARD_TOK_STRING, str.items, loc);
    return 0;
}

int shard_lex(struct shard_context* ctx, struct shard_source* src, struct shard_token* token) {
    int c;

repeat:

    while(isspace(c = src->getc(src))) {
        if(c == '\n')
            src->line++;
    }

    switch(c) {
        case EOF:
            EOF_TOK(token, src);
            break;
        case '#':
            while((c = src->getc(src)) != '\n' && c != EOF);
            src->line++;
            goto repeat;
        case '(':
            KEYWORD_TOK(token, src, LPAREN);
            break;
        case ')':
            KEYWORD_TOK(token, src, RPAREN);
            break;
        case '[':
            KEYWORD_TOK(token, src, LBRACKET);
            break;
        case ']':
            KEYWORD_TOK(token, src, RBRACKET);
            break;
        case '{':
            KEYWORD_TOK(token, src, LBRACE);
            break;
        case '}':
            KEYWORD_TOK(token, src, RBRACE);
            break;
        case '+':
            if((c = src->getc(src)) == '+')
                KEYWORD_TOK(token, src, CONCAT);
            else {
                src->ungetc(c, src);
                KEYWORD_TOK(token, src, ADD);
            }
            break;
        case '-':
            if((c = src->getc(src)) == '>')
                KEYWORD_TOK(token, src, LOGIMPL);
            else {
                src->ungetc(c, src);
                KEYWORD_TOK(token, src, SUB);
            }
            break;
        case '*':
            KEYWORD_TOK(token, src, MUL);
            break;
        case '/':
            if((c = src->getc(src)) == '/')
                KEYWORD_TOK(token, src, MERGE);
            else if(!isspace(c)) {
                src->ungetc(c, src);
                return lex_string(ctx, src, token, isspace, PATH_ABS);
            }
            else {
                src->ungetc(c, src);
                KEYWORD_TOK(token, src, DIV);
            }
            break;
        case '~':
            return lex_string(ctx, src, token, isspace, PATH_HOME);
        case ':':
            KEYWORD_TOK(token, src, COLON);
            break;
        case ';':
            KEYWORD_TOK(token, src, SEMICOLON);
            break;
        case '&':
            if((c = src->getc(src)) == '&')
                KEYWORD_TOK(token, src, LOGAND);
            else {
                src->ungetc(c, src);
                ERR_TOK(token, src, "unknown token `&`, did you mean `&&`?");
                return EINVAL;
            }
            break;
        case '|':
            if((c = src->getc(src)) == '|')
                KEYWORD_TOK(token, src, LOGOR);
            else {
                src->ungetc(c, src);
                ERR_TOK(token, src, "unknownt token `|`, did you mean `||`?");
                return EINVAL;
            }
            break;
        case '.': {
            int c2 = -2;
            if((c = src->getc(src)) == '.' && (c2 = src->getc(src)) == '.')
                KEYWORD_TOK(token, src, ELLIPSE);
            else if(c == '/') {
                if(c2 != -2)
                    src->ungetc(c2, src);
                return lex_string(ctx, src, token, isspace, PATH_REL);
            }
            else {
                if(c2 != -2)
                    src->ungetc(c2, src);
                src->ungetc(c, src);
                KEYWORD_TOK(token, src, PERIOD);
            }
        } break;
        case '?':
            KEYWORD_TOK(token, src, QUESTIONMARK);
            break;
        case '!':
            if((c = src->getc(src)) == '=')
                KEYWORD_TOK(token, src, NE);
            else {
                src->ungetc(c, src);
                KEYWORD_TOK(token, src, EXCLAMATIONMARK);
            }
            break;
        case '>':
            if((c = src->getc(src)) == '=')
                KEYWORD_TOK(token, src, GE);
            else {
                src->ungetc(c, src);
                KEYWORD_TOK(token, src, GT);
            }
            break;
        case '<':
            if((c = src->getc(src)) == '=')
                KEYWORD_TOK(token, src, LE);
            else if(!isspace(c)) {
                src->ungetc(c, src);
                return lex_string(ctx, src, token, is_gt_sign, PATH_INC);
            }
            else {
                src->ungetc(c, src);
                KEYWORD_TOK(token, src, LT);
            }
            break;
        case '@':
            KEYWORD_TOK(token, src, AT);
            break;
        case '=':
            if((c = src->getc(src)) == '=')
                KEYWORD_TOK(token, src, EQ);
            else {
                src->ungetc(c, src);
                KEYWORD_TOK(token, src, ASSIGN);
            }
            break;
        case '\'':
            if((c = src->getc(src)) == '\'')
                return lex_multiline_string(ctx, src, token);
            else {
                ERR_TOK(token, src, "unexpected character, expect second `'` for multiline string");
                return EINVAL;
            }
        case '\"':
            return lex_string(ctx, src, token, is_double_quote, PATH_NONE);
        default:
            if(isdigit(c)) {
                src->ungetc(c, src);
                return lex_number(ctx, src, token);
            }
            else if(is_ident_char(c)) {
                src->ungetc(c, src);
                return lex_ident(ctx, src, token);
            }
            else
                ERR_TOK(token, src, "Unexpected character");
            return EINVAL;
    }

    return 0;
}

