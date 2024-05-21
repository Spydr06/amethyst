#define _LIBSHARD_INTERNAL
#include <libshard.h>

#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>
#include <stddef.h>

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

#define STRING_TOK(tok, src, str, loc) (            \
    init_token((tok), SHARD_TOK_STRING,             \
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
    PATH_ABS
};

static char tmpbuf[BUFSIZ + 1];

static const unsigned token_widths[] = {
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
    1,
    1,
    1,
    2,
    2,
    1,
    1,
    1,
    1,
    1,
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
    unsigned start = src->tell(src) - 1;

    int c, i = 0;
    while(is_ident_char(c = src->getc(src))) {
        if(i >= BUFSIZ) {
            ERR_TOK(token, src, "identifier too long");
            return EOVERFLOW;
        }

        tmpbuf[i++] = (char) c;
    }

    unsigned end = src->tell(src);
    
    tmpbuf[i] = '\0';
    src->ungetc(c, src);

    enum shard_token_type type = get_keyword(tmpbuf);
    if(type == SHARD_TOK_IDENT) {
        char* word = arena_malloc(ctx, ctx->idents, end - start);
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

static int lex_number(struct shard_context* __attribute__((unused)) ctx, struct shard_source* src, struct shard_token* token) {
    unsigned start = src->tell(src);

    bool floating = false;
    int c, i = 0;
    while(isdigit(c = src->getc(src)) || c == '\'' || c == '.') {
        if(i >= BUFSIZ) {
            ERR_TOK(token, src, "number literal too long");
            return EOVERFLOW;
        }

        switch(c) {
            case '\'':
                break;
            case '.':
                if(floating) {
                    ERR_TOK(token, src, "more than one period (`.`) in number literal");
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

    errno = 0;
    double d = strtod(tmpbuf, NULL);
    if(errno) {
        ERR_TOK(token, src, strerror(errno));
        return errno;
    }

    struct shard_location loc = {
        .src = src,
        .line = src->line,
        .offset = start,
        .width = end - start
    };

    init_token(token, SHARD_TOK_NUMBER, loc, (union shard_token_value){.number = d});
    return 0;
}

static int is_double_quote(int c) {
    return c == '"';
}

static int is_single_quote(int c) {
    return c == '\'';
}

static int is_gt_sign(int c) {
    return c == '>';
}

static int lex_string(struct shard_context* ctx, struct shard_source* src, struct shard_token* token, int (*is_end_quote)(int), enum path_mode pmode) {
    unsigned start = src->tell(src) - 1;
    
    struct shard_string str = {0};

    int last = 0, c;
    while(!is_end_quote(c = src->getc(src)) || last == '\\') {
        dynarr_append(ctx, &str, c);
        last = c;

        switch(c) {
        case '\n':
            src->line++;
            break;
        case EOF:
            dynarr_free(ctx, &str);
            ERR_TOK(token, src, "unterminated string literal");
            return EINVAL;
        }
    }

    switch(pmode) {
        case PATH_NONE:
        case PATH_ABS:
            break;
        case PATH_REL:
            // TODO: relative to absolute path
            break;
        case PATH_INC:
            // TODO: include path to absolute path
            break;
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

    STRING_TOK(token, src, str.items, loc);
    return 0;
}

int shard_lex(struct shard_context* ctx, struct shard_source* src, struct shard_token* token) {
    int c;

repeat:;

    while(isspace(c = src->getc(src))) {
        if(c == '\n')
            src->line++;
    }

    switch(c) {
        case EOF:
            EOF_TOK(token, src);
            break;
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
            if((c = src->getc(src)) == '-') {
                for(;;) {
                    while((c = src->getc(src)) != '-') {
                        switch(c) {
                            case '\n':
                                src->line++;
                            case EOF:
                                ERR_TOK(token, src, "unterminated block comment, expect `-}`");
                                return EOVERFLOW;
                        }
                    }
                    if((c = src->getc(src)) == '}')
                        goto repeat;
                }
            }
            else {
                src->ungetc(c, src);
                KEYWORD_TOK(token, src, LBRACE);
            }
            break;
        case '}':
            KEYWORD_TOK(token, src, RBRACE);
            break;
        case '+':
            if((c = src->getc(src)) == '+')
                KEYWORD_TOK(token, src, MERGE);
            else {
                src->ungetc(c, src);
                KEYWORD_TOK(token, src, ADD);
            }
            break;
        case '-':
            if((c = src->getc(src)) == '-') {
                while((c = src->getc(src)) != '\n' && c != EOF);
                src->line++;
                goto repeat;
            }
            else {
                src->ungetc(c, src);
                KEYWORD_TOK(token, src, SUB);
            }
            break;
        case '/':
            return lex_string(ctx, src, token, isspace, PATH_ABS);
        case ':':
            KEYWORD_TOK(token, src, COLON);
            break;
        case ';':
            KEYWORD_TOK(token, src, SEMICOLON);
            break;
        case '.':
            if((c = src->getc(src)) == '.')
                KEYWORD_TOK(token, src, ELLIPSE);
            else if(c == '/')
                return lex_string(ctx, src, token, isspace, PATH_REL);
            else {
                src->ungetc(c, src);
                KEYWORD_TOK(token, src, PERIOD);
            }
            break;
        case '?':
            KEYWORD_TOK(token, src, QUESTIONMARK);
            break;
        case '!':
            KEYWORD_TOK(token, src, EXCLAMATIONMARK);
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
            return lex_string(ctx, src, token, is_single_quote, PATH_NONE);
        case '\"':
            return lex_string(ctx, src, token, is_double_quote, PATH_NONE);
        case '<':
            return lex_string(ctx, src, token, is_gt_sign, PATH_INC);
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

