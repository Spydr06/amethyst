#define _LIBSHARD_INTERNAL
#include <libshard.h>

#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>
#include <stddef.h>

#define PATH_MAX 4096

#define LOC_FROM_LEXER(l, w) ((struct shard_location) { \
        .src = (l)->current_loc.src,                    \
        .line = (l)->current_loc.line,                  \
        .width = (w),                                   \
        .offset = (l)->current_loc.offset - (w)         \
    })

#define BASIC_TOK(tok, l, type) (                 \
    init_token((tok), (type),                       \
        LOC_FROM_LEXER(l, token_widths[(type)]), \
        (union shard_token_value){.string = NULL}   \
    ))

#define STRING_TOK(tok, l, type, str, loc) (      \
    init_token((tok), (type),                       \
        (loc),                                      \
        (union shard_token_value){.string = (str)}  \
    ))

#define ERR_TOK(tok, l, err) (                    \
    init_token((tok), SHARD_TOK_ERR,                \
        LOC_FROM_LEXER(l, 1),                    \
        (union shard_token_value){.string = (err)}  \
    ))

#define KEYWORD_TOK(tok, l, kind) BASIC_TOK(tok, l, SHARD_TOK_##kind)
#define EOF_TOK(tok, l) KEYWORD_TOK(tok, l, EOF);

struct shard_lexer {
    struct shard_context* ctx;
    struct shard_source* src;
    struct shard_location current_loc;
};

static char tmpbuf[PATH_MAX + 1];

static const unsigned token_widths[] = {
    0, // eof
    0, // ident
    0, // string
    0, // path
    0, // int
    0, // float
    1, // (
    1, // )
    1, // [
    1, // ]
    1, // {
    1, // }
    1, // =
    2, // ==
    2, // !=
    1, // >
    2, // >=
    1, // <
    2, // <=
    2, // <<
    2, // >>
    1, // :
    2, // ::
    1, // ;
    1, // ,
    1, // .
    3, // ...
    2, // //
    2, // ++
    1, // ?
    1, // !
    1, // @
    1, // +
    1, // -
    1, // *
    1, // /
    1, // |
    2, // &&
    2, // ||
    2, // ->
    2, // =>
    1, // $
    2, // ${
    3, // rec
    2, // or
    2, // if
    4, // then
    4, // else
    6, // assert
    3, // let
    2, // in
    4, // with
    7, // inherit
    4, // case
    2, // of
};

static_assert((sizeof(token_widths) / sizeof(unsigned)) == _SHARD_TOK_LEN);

static const struct {
    const char* keyword;
    enum shard_token_type type;
} keywords[] = {
    {"rec", SHARD_TOK_REC},
    {"or", SHARD_TOK_OR},
    {"if", SHARD_TOK_IF},
    {"then", SHARD_TOK_THEN},
    {"else", SHARD_TOK_ELSE},
    {"assert", SHARD_TOK_ASSERT},
    {"let", SHARD_TOK_LET},
    {"in", SHARD_TOK_IN},
    {"with", SHARD_TOK_WITH},
    {"inherit", SHARD_TOK_INHERIT},
    {"case", SHARD_TOK_CASE},
    {"of", SHARD_TOK_OF},
    {NULL, SHARD_TOK_EOF},
};

static inline struct shard_location default_loc(struct shard_source* src) {
    return (struct shard_location){
        .src = src,
        .line = 1,
        .offset = 0,
        .width = 0
    };
}

struct shard_lexer* shard_lex_init(struct shard_context* ctx, struct shard_source* src) {
    struct shard_lexer* l = ctx->malloc(sizeof(struct shard_lexer));

    l->ctx = ctx;
    l->src = src;
    l->current_loc = default_loc(src);

    return l;
}

void shard_lex_free(struct shard_lexer* l) {
    l->ctx->free(l);
}

static inline int l_getc(struct shard_lexer* l) {
    l->current_loc.offset++;
    int c = l->src->getc(l->src);
    if(c == '\n')
        l->current_loc.line++;
    return c;
}

static inline int l_ungetc(int c, struct shard_lexer* l) {
    assert(l->current_loc.offset != 0);
    l->current_loc.offset--;
    if(c == '\n') {
        assert(l->current_loc.line != 0);
        l->current_loc.line--;
    }
    return l->src->ungetc(c, l->src);
}

static inline int l_offset(struct shard_lexer* l) {
    return l->current_loc.offset;
}

static inline int l_line(struct shard_lexer* l) {
    return l->current_loc.line;
}

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

static int is_path_terminator(int c) {
    return isspace(c) || c == ';' || c == ',' || c == ')' || c == ']' || c == '}';
}

static int lex_ident(struct shard_lexer* l, struct shard_token* token) {
    unsigned start = l_offset(l);

    int c, i = 0;
    while(is_ident_char(c = l_getc(l))) {
        if(i >= (int) LEN(tmpbuf)) {
            ERR_TOK(token, l, "identifier too long");
            return ENAMETOOLONG;
        }

        tmpbuf[i++] = (char) c;
    }

    unsigned end = l_offset(l);
    
    tmpbuf[i] = '\0';
    l_ungetc(c, l);

    enum shard_token_type type = get_keyword(tmpbuf);
    if(type == SHARD_TOK_IDENT) {
        shard_ident_t ident = shard_get_ident(l->ctx, tmpbuf);

        struct shard_location loc = {
            .src = l->src,
            .line = l_line(l),
            .offset = start,
            .width = end - start - 1
        };

        init_token(token, type, loc, (union shard_token_value){.string = (char*) ident});
    }
    else
        BASIC_TOK(token, l, type);
    return 0;
}

static int lex_number(struct shard_lexer* l, struct shard_token* token) {
    unsigned start = l_offset(l);

    int64_t integer = 0;
    int64_t fractional = 0;
    int64_t divisor = 1;

    bool floating = false;
    int c, i = 0;
    while(isdigit(c = l_getc(l)) || c == '\'' || c == '.') {
        if(i >= (int) LEN(tmpbuf)) {
            ERR_TOK(token, l, "floating literal too long");
            return ENAMETOOLONG;
        }

        switch(c) {
            case '\'':
                break;
            case '.':
                if(floating) {
                    ERR_TOK(token, l, "more than one period (`.`) in floating literal");
                    return EINVAL;
                }
                floating = true;
                break;
            default:
                if(floating) {
                    fractional = fractional * 10 + (c - '0');
                    divisor *= 10;
                }
                else 
                    integer = integer * 10 + (c - '0');
        }
    }

    tmpbuf[i] = '\0';
    l_ungetc(c, l);
    
    unsigned end = l_offset(l);

    struct shard_location loc = {
        .src = l->src,
        .line = l_line(l),
        .offset = start,
        .width = end - start
    };

    errno = 0;
    if(floating) {
        init_token(token, SHARD_TOK_FLOAT, loc, (union shard_token_value){.floating = ((double) integer) + (((double) fractional) / ((double) divisor)) });
    }
    else {
        init_token(token, SHARD_TOK_INT, loc, (union shard_token_value){.integer = integer});
    }
    return 0;
}

static int is_double_quote(int c) {
    return c == '"';
}

static int is_gt_sign(int c) {
    return c == '>';
}

static int lex_string(struct shard_lexer* l, struct shard_token* token, int (*is_end_quote)(int), bool is_path) {
    unsigned start = l_offset(l) - 1;
    
    struct shard_string str = {0};

    bool escaped = false;
    int c;
    while(!is_end_quote(c = l_getc(l)) || escaped) {
        dynarr_append(l->ctx, &str, c);

        escaped = false;
        switch(c) {
        case '\\':
            escaped = true;
            break;
        case '\n':
            ERR_TOK(token, l, "unterminated string literal before newline");
            break;
        case EOF:
            dynarr_free(l->ctx, &str);
            ERR_TOK(token, l, "unterminated string literal");
            return EINVAL;
        }
    }
    if(is_path)
        l_ungetc(c, l);

    dynarr_append(l->ctx, &str, '\0');

    dynarr_append(l->ctx, &l->ctx->string_literals, str.items);

    unsigned end = l_offset(l);
    struct shard_location loc = {
        .src = l->src,
        .line = l_line(l),
        .offset = start - 1,
        .width = end - start + 1
    };

    STRING_TOK(token, src, is_path ? SHARD_TOK_PATH : SHARD_TOK_STRING, str.items, loc);
    return 0;
}

static int lex_multiline_string(struct shard_lexer* l, struct shard_token* token) {
    unsigned start = l_offset(l) - 1;

    struct shard_string str = {0};

    bool escaped[3] = {0};
    int c[2] = {0, 0};
    for(;;) {
        c[1] = c[0];
        c[0] = l_getc(l);

        escaped[2] = escaped[1];
        escaped[1] = escaped[0];
        escaped[0] = false;
        
        switch(c[0]) {
            case '\'':
                if(escaped[1] || escaped[2])
                    dynarr_append(l->ctx, &str, '\'');
                else if(c[1] == '\'')
                    goto break_loop;
                break;
            case '\n':
                dynarr_append(l->ctx, &str, '\n');
                break;
            case '\\':
                escaped[0] = true;
                dynarr_append(l->ctx, &str, '\\');
                break;
            case EOF:
                dynarr_free(l->ctx, &str);
                ERR_TOK(token, l, "unterminated multiline string literal, expect `''`");
                return EINVAL;
            default:
                dynarr_append(l->ctx, &str, c[0]);
        }
    }

break_loop:

    dynarr_append(l->ctx, &str, '\0');
    dynarr_append(l->ctx, &l->ctx->string_literals, str.items);

    unsigned end = l_offset(l);
    struct shard_location loc = {
        .src = l->src,
        .line = l_line(l),
        .offset = start - 1,
        .width = end - start + 1
    };

    STRING_TOK(token, src, SHARD_TOK_STRING, str.items, loc);
    return 0;
}

int shard_lex(struct shard_lexer* l, struct shard_token* token) {
    int c;

repeat:

    while(isspace(c = l_getc(l)));

    switch(c) {
        case EOF:
            EOF_TOK(token, l);
            break;
        case '#':
            while((c = l_getc(l)) != '\n' && c != EOF);
            goto repeat;
        case '(':
            KEYWORD_TOK(token, l, LPAREN);
            break;
        case ')':
            KEYWORD_TOK(token, l, RPAREN);
            break;
        case '[':
            KEYWORD_TOK(token, l, LBRACKET);
            break;
        case ']':
            KEYWORD_TOK(token, l, RBRACKET);
            break;
        case '{':
            KEYWORD_TOK(token, l, LBRACE);
            break;
        case '}':
            KEYWORD_TOK(token, l, RBRACE);
            break;
        case '+':
            if((c = l_getc(l)) == '+')
                KEYWORD_TOK(token, l, CONCAT);
            else {
                l_ungetc(c, l);
                KEYWORD_TOK(token, l, ADD);
            }
            break;
        case '-':
            if((c = l_getc(l)) == '>')
                KEYWORD_TOK(token, l, LOGIMPL);
            else {
                l_ungetc(c, l);
                KEYWORD_TOK(token, l, SUB);
            }
            break;
        case '*':
            KEYWORD_TOK(token, l, MUL);
            break;
        case '/':
            if((c = l_getc(l)) == '/')
                KEYWORD_TOK(token, l, MERGE);
            else if(c == '*') {
                for(;;) {
                    while((c = l_getc(l)) != '*') {
                        if(c == EOF) {
                            ERR_TOK(token, l, "unterminated multiline comment, expect `*/`");
                            return EOVERFLOW;
                        }
                    }
                    if((c = l_getc(l)) == '/')
                        goto repeat;
                }
            }
            else if(!isspace(c)) {
                l_ungetc(c, l);
                l_ungetc('/', l);
                return lex_string(l, token, is_path_terminator, true);
            }
            else {
                l_ungetc(c, l);
                KEYWORD_TOK(token, l, DIV);
            }
            break;
        case '~':
            l_ungetc(c, l);
            return lex_string(l, token, is_path_terminator, true);
        case ':':
            if((c = l_getc(l)) == ':')
                KEYWORD_TOK(token, l, DOUBLE_COLON);
            else {
                l_ungetc(c, l);
                KEYWORD_TOK(token, l, COLON);
            }
            break;
        case ';':
            KEYWORD_TOK(token, l, SEMICOLON);
            break;
        case '&':
            if((c = l_getc(l)) == '&')
                KEYWORD_TOK(token, l, LOGAND);
            else {
                l_ungetc(c, l);
                ERR_TOK(token, l, "unknown token `&`, did you mean `&&`?");
                return EINVAL;
            }
            break;
        case '|':
            if((c = l_getc(l)) == '|')
                KEYWORD_TOK(token, l, LOGOR);
            else {
                l_ungetc(c, l);
                KEYWORD_TOK(token, l, PIPE);
            }
            break;
        case ',':
            KEYWORD_TOK(token, l, COMMA);
            break;
        case '.': {
            int c2 = -2;
            if((c = l_getc(l)) == '.' && (c2 = l_getc(l)) == '.')
                KEYWORD_TOK(token, l, ELLIPSE);
            else if(c == '.' && c2 == '/') {
                l_ungetc('/', l);
                l_ungetc('.', l);
                l_ungetc('.', l);
                return lex_string(l, token, is_path_terminator, true);
            }
            else if(c == '/') {
                if(c2 != -2)
                    l_ungetc(c2, l);
                l_ungetc('/', l);
                l_ungetc('.', l);
                return lex_string(l, token, is_path_terminator, true);
            }
            else {
                if(c2 != -2)
                    l_ungetc(c2, l);
                l_ungetc(c, l);
                KEYWORD_TOK(token, l, PERIOD);
            }
        } break;
        case '?':
            KEYWORD_TOK(token, l, QUESTIONMARK);
            break;
        case '!':
            if((c = l_getc(l)) == '=')
                KEYWORD_TOK(token, l, NE);
            else {
                l_ungetc(c, l);
                KEYWORD_TOK(token, l, EXCLAMATIONMARK);
            }
            break;
        case '>':
            if((c = l_getc(l)) == '=')
                KEYWORD_TOK(token, l, GE);
            else if(c == '>')
                KEYWORD_TOK(token, l, RCOMPOSE);
            else {
                l_ungetc(c, l);
                KEYWORD_TOK(token, l, GT);
            }
            break;
        case '<':
            if((c = l_getc(l)) == '=')
                KEYWORD_TOK(token, l, LE);
            else if(c == '<')
                KEYWORD_TOK(token, l, LCOMPOSE);
            else if(!isspace(c)) {
                l_ungetc(c, l);
                int err = lex_string(l, token, is_gt_sign, false);
                token->type = SHARD_TOK_PATH;
                return err;
            }
            else {
                l_ungetc(c, l);
                KEYWORD_TOK(token, l, LT);
            }
            break;
        case '@':
            KEYWORD_TOK(token, l, AT);
            break;
        case '$':
            KEYWORD_TOK(token, l, DOLLAR);
            break;
        case '=':
            if((c = l_getc(l)) == '=')
                KEYWORD_TOK(token, l, EQ);
            else if(c == '>')
                KEYWORD_TOK(token, l, ARROW);
            else {
                l_ungetc(c, l);
                KEYWORD_TOK(token, l, ASSIGN);
            }
            break;
        case '\'':
            if((c = l_getc(l)) == '\'')
                return lex_multiline_string(l, token);
            else {
                ERR_TOK(token, l, "unexpected character, expect second `'` for multiline string");
                return EINVAL;
            }
        case '\"':
            return lex_string(l, token, is_double_quote, false);
        default:
            if(isdigit(c)) {
                l_ungetc(c, l);
                return lex_number(l, token);
            }
            else if(is_ident_char(c)) {
                l_ungetc(c, l);
                return lex_ident(l, token);
            }
            else
                ERR_TOK(token, l, "Unexpected character");
            return EINVAL;
    }

    return 0;
}

