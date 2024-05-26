#define _LIBSHARD_INTERNAL
#include <libshard.h>

#include <stdio.h>

#define E(t) [SHARD_TOK_##t]

static const char* token_type_strings[_SHARD_TOK_LEN] = {
    E(EOF) = "<end of file>",
    E(IDENT) = "identifier",
    E(PATH) = "path literal",
    E(STRING) = "string literal",
    E(INT) = "integer literal",
    E(FLOAT) = "floating literal",
    E(LPAREN) = "(",
    E(RPAREN) = ")",
    E(LBRACKET) = "[",
    E(RBRACKET) = "]",
    E(LBRACE) = "{",
    E(RBRACE) = "}",
    E(ASSIGN) = "=",
    E(EQ) = "==",
    E(NE) = "!=",
    E(GT) = ">",
    E(GE) = ">=",
    E(LT) = "<",
    E(LE) = "<=",
    E(COLON) = ":",
    E(SEMICOLON) = ";",
    E(COMMA) = ",",
    E(PERIOD) = ".",
    E(ELLIPSE) = "...",
    E(MERGE) = "//",
    E(CONCAT) = "++",
    E(QUESTIONMARK) = "?",
    E(EXCLAMATIONMARK) = "!",
    E(AT) = "@",
    E(ADD) = "+",
    E(SUB) = "-",
    E(MUL) = "*",
    E(DIV) = "/",
    E(LOGAND) = "&&",
    E(LOGOR) = "||",
    E(LOGIMPL) = "->",
    E(NULL) = "null",
    E(TRUE) = "true",
    E(FALSE) = "false",
    E(REC) = "rec",
    E(OR) = "or",
    E(IF) = "if",
    E(THEN) = "then",
    E(ELSE) = "else",
    E(ASSERT) = "assert",
    E(LET) = "let",
    E(IN) = "in",
    E(WITH) = "with"
};

#undef E

const char* shard_token_type_to_str(enum shard_token_type token_type) {
    return token_type < 0 ? "<error token>" : token_type_strings[token_type];
}

void shard_dump_token(char* dest, size_t n, const struct shard_token* tok) {
    switch(tok->type) {
        case SHARD_TOK_ERR:
            snprintf(dest, n, "<error: %s>", tok->value.string);
            break;
        case SHARD_TOK_IDENT:
            strncpy(dest, tok->value.string, n);
            break;
        case SHARD_TOK_STRING:
            snprintf(dest, n, "\"%s\"", tok->value.string);
            break;
        case SHARD_TOK_PATH:
            strncpy(dest, tok->value.string, n);
            break;
        case SHARD_TOK_FLOAT:
            snprintf(dest, n, "%f", tok->value.floating);
            break;
        case SHARD_TOK_INT:
            snprintf(dest, n, "%ld", tok->value.integer);
            break;
        default:
            if(token_type_strings[tok->type])
                strncpy(dest, token_type_strings[tok->type], n);
            else
                strncpy(dest, "<unknown>", n);
    }
}

#define BOP(name, _str) case SHARD_EXPR_##name:                     \
        shard_dump_expr(ctx, str, expr->binop.lhs);                 \
        dynarr_append_many(ctx, str, " " _str " ", LEN(_str) + 1);  \
        shard_dump_expr(ctx, str, expr->binop.rhs);                 \
        break

void shard_dump_expr(struct shard_context* ctx, struct shard_string* str, const struct shard_expr* expr) {
    dynarr_append(ctx, str, '(');
    
    switch(expr->type) {
        case SHARD_EXPR_NULL:
            dynarr_append_many(ctx, str, "null", 4);
            break;
        case SHARD_EXPR_TRUE:
            dynarr_append_many(ctx, str, "true", 4);
            break;
        case SHARD_EXPR_FALSE:
            dynarr_append_many(ctx, str, "false", 5);
            break;
        case SHARD_EXPR_IDENT:
            dynarr_append_many(ctx, str, expr->string, strlen(expr->string));
            break;
        case SHARD_EXPR_STRING:
            dynarr_append(ctx, str, '"');
            dynarr_append_many(ctx, str, expr->string, strlen(expr->string));
            dynarr_append(ctx, str, '"');
            break;
        case SHARD_EXPR_PATH:
            dynarr_append_many(ctx, str, expr->string, strlen(expr->string));
            break;
        case SHARD_EXPR_FLOAT: {
            char buf[32];
            snprintf(buf, 32, "%f", expr->floating);
            dynarr_append_many(ctx, str, buf, strlen(buf));
            break;
        }
        case SHARD_EXPR_INT: {
            char buf[32];
            snprintf(buf, 32, "%ld", expr->integer);
            dynarr_append_many(ctx, str, buf, strlen(buf));
            break;
        }
        case SHARD_EXPR_WITH:
            dynarr_append_many(ctx, str, "with ", 5);
            shard_dump_expr(ctx, str, expr->binop.lhs);
            dynarr_append_many(ctx, str, "; ", 2);
            shard_dump_expr(ctx, str, expr->binop.rhs);
            break;
        case SHARD_EXPR_ASSERT:
            dynarr_append_many(ctx, str, "assert ", 7);
            shard_dump_expr(ctx, str, expr->binop.lhs);
            dynarr_append_many(ctx, str, "; ", 2);
            shard_dump_expr(ctx, str, expr->binop.rhs);
            break;
        case SHARD_EXPR_NEGATE:
            dynarr_append(ctx, str, '-');
            shard_dump_expr(ctx, str, expr->unaryop.expr);
            break;
        case SHARD_EXPR_NOT:
            dynarr_append(ctx, str, '-');
            shard_dump_expr(ctx, str, expr->unaryop.expr);
            break;
        case SHARD_EXPR_TERNARY:
            dynarr_append_many(ctx, str, "if ", 3);
            shard_dump_expr(ctx, str, expr->ternary.cond);
            dynarr_append_many(ctx, str, " then ", 6);
            shard_dump_expr(ctx, str, expr->ternary.if_branch);
            dynarr_append_many(ctx, str, " else ", 6);
            shard_dump_expr(ctx, str, expr->ternary.else_branch);
            break;
        BOP(ADD, "+");
        BOP(SUB, "-");
        BOP(MUL, "*");
        BOP(DIV, "/");
        BOP(EQ, "==");
        BOP(NE, "!=");
        BOP(GT, ">");
        BOP(GE, ">=");
        BOP(LT, "<");
        BOP(LE, "<=");
        BOP(MERGE, "//");
        BOP(CONCAT, "++");
        BOP(LOGOR, "||");
        BOP(LOGAND, "&&");
        BOP(LOGIMPL, "->");
        case SHARD_EXPR_CALL:
            shard_dump_expr(ctx, str, expr->binop.lhs);
            dynarr_append(ctx, str, ' ');
            shard_dump_expr(ctx, str, expr->binop.rhs);
            break;
        case SHARD_EXPR_LIST:
            dynarr_append(ctx, str, '[');
            dynarr_append(ctx, str, ' ');
            for(size_t i = 0; i < expr->list.elems.count; i++) {
                shard_dump_expr(ctx, str, &expr->list.elems.items[i]);
                dynarr_append(ctx, str, ' ');
            }
            dynarr_append(ctx, str, ']');
            break;
        case SHARD_EXPR_SET:
            if(expr->set.recursive)
                dynarr_append_many(ctx, str, "rec ", 4);
            dynarr_append(ctx, str, '{');
            dynarr_append(ctx, str, ' ');
            for(size_t i = 0; i < expr->set.attrs.alloc; i++) {
                const struct shard_hashpair* pair = &expr->set.attrs.pairs[i];
                if(!pair->value)
                    continue;
                
                dynarr_append_many(ctx, str, pair->key, strlen(pair->key));
                dynarr_append_many(ctx, str, " = ", 3);
                shard_dump_expr(ctx, str, pair->value);
                dynarr_append(ctx, str, ';');
                dynarr_append(ctx, str, ' ');
            }
            dynarr_append(ctx, str, '}');
            break;
        case SHARD_EXPR_ATTR_SEL:
            shard_dump_expr(ctx, str, expr->attr_sel.set);
            dynarr_append_many(ctx, str, " . <attr path>", 14);
            if(expr->attr_sel.default_value) {
                dynarr_append_many(ctx, str, " or ", 4);
                shard_dump_expr(ctx, str, expr->attr_sel.default_value);
            }
            break;
        case SHARD_EXPR_ATTR_TEST:
            shard_dump_expr(ctx, str, expr->attr_test.set);
            dynarr_append_many(ctx, str, " . <attr path>", 14);
            break;
        case SHARD_EXPR_FUNCTION:
            shard_dump_pattern(ctx, str, expr->func.arg);
            dynarr_append(ctx, str, ':');
            dynarr_append(ctx, str, ' ');
            shard_dump_expr(ctx, str, expr->func.body);
            break;
        default:
            dynarr_append_many(ctx, str, "<unknown>", 9);
    }

    dynarr_append(ctx, str, ')');
}

#undef BOP

void shard_dump_pattern(struct shard_context* ctx, struct shard_string* str, const struct shard_pattern* pattern) {
    switch(pattern->type) {
        case SHARD_PAT_IDENT:
            dynarr_append_many(ctx, str, pattern->ident, strlen(pattern->ident));
            break;
        case SHARD_PAT_SET:
            dynarr_append(ctx, str, '{');
            dynarr_append(ctx, str, ' ');
            for(size_t i = 0; i < pattern->attrs.count; i++) {
                struct shard_pattern_attr* attr = &pattern->attrs.items[i];
                dynarr_append_many(ctx, str, attr->ident, strlen(attr->ident));
                
                if(attr->default_value) {
                    dynarr_append_many(ctx, str, " ? ", 3);
                    shard_dump_expr(ctx, str, attr->default_value);
                }

                if(pattern->attrs.count - i > 1)
                    dynarr_append(ctx, str, ',');
                dynarr_append(ctx, str, ' ');
            }

            if(pattern->ellipsis)
                dynarr_append_many(ctx, str, "... ", 4);

            dynarr_append(ctx, str, '}');

            if(pattern->ident) {
                dynarr_append_many(ctx, str, " @ ", 3);
                dynarr_append_many(ctx, str, pattern->ident, strlen(pattern->ident));
            }
            break;
        default:
            dynarr_append_many(ctx, str, "<unknown pattern>", 17);
    }
}
