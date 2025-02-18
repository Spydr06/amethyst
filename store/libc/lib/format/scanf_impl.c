#include <stdio.h>

#include <internal/format.h>

#define _GNU_SOURCE
#include <assert.h>
#include <ctype.h>
#include <stdbool.h>
#include <string.h>

struct scanf_state {
    fmtscanner_t scanner;
    void *userp;
    va_list ap;

    bool errored;
    int num_assigned;
    size_t bytes_read;

    char last;
    char unwound;
};

struct conversion {
    bool assignment_suppressed;
    int width;
    enum length_modifier length_modifier;
    char specifier;
};

static signed char _next_char(struct scanf_state *state) {
    if(state->unwound > 0) {
        state->last = state->unwound;
        state->unwound = EOF;
        return state->last;
    }

    ssize_t read = state->scanner(state->userp, &state->last, sizeof(char));
    if(read > 0) {
        state->bytes_read++;
        return state->last;
    }
    return read;
}

// FIXME: maybe synchronize with scanner? (could mess up file offsets)
static void _unwind_once(struct scanf_state *state) {
    assert(state->unwound == EOF);
    state->unwound = state->last;
}

static void _skip_whitespace(struct scanf_state *state) {
    while(isspace(_next_char(state)));
    _unwind_once(state);
}

static bool _expect_char(struct scanf_state *state, char c) {
    if(_next_char(state) != c) {
        state->errored = true;
        return false;
    }

    return true;
}

static void _scan(const char *restrict str, size_t size, struct scanf_state *state) {
    for(size_t i = 0; i < size; i++) {
        char c = str[i];
        if(isspace(c)) {
            _skip_whitespace(state);
            continue;
        }
         
        if(!_expect_char(state, c))
            break;
    }
}

static void _parse_width(struct conversion *conv, const char *restrict *format) {
    conv->width = 0;
    for(; isdigit(**format); (*format)++)
        conv->width = conv->width * 10 + **format - '0';
}

static void _assign_int(struct conversion *conv, struct scanf_state *state, intmax_t i) {
    if(conv->assignment_suppressed)
        return;

    state->num_assigned++;
    switch(conv->length_modifier) {
        case LMOD_CHAR:
            *va_arg(state->ap, char*) = (char) i;
            break;
        case LMOD_SHORT:
            *va_arg(state->ap, short*) = (short) i;
            break;
        case LMOD_LONG:
            *va_arg(state->ap, long*) = (long) i;
            break;
        case LMOD_LONG_DOUBLE:
        case LMOD_LONG_LONG:
            *va_arg(state->ap, long long*) = (long long) i;
            break;
        case LMOD_INTMAX:
            *va_arg(state->ap, intmax_t*) = i;
            break;
        case LMOD_SIZE:
            *va_arg(state->ap, size_t*) = (size_t) i;
            break;
        case LMOD_PTRDIFF:
            *va_arg(state->ap, ptrdiff_t*) = (ptrdiff_t) i;
    }
}

static void _char_conversion(struct conversion *conv, struct scanf_state *state) {
    if(conv->width <= 0)
        conv->width = 1;

    if(conv->assignment_suppressed) {
        for(int i = 0; i < conv->width; i++)
            _next_char(state);
        return;
    }

    state->num_assigned++;
    char *dst = va_arg(state->ap, char*);
    
    for(int i = 0; i < conv->width; i++)
        dst[i] = _next_char(state);
}

static void _string_conversion(struct conversion *conv, struct scanf_state *state) {
    char *dst = conv->assignment_suppressed ? NULL : va_arg(state->ap, char*);

    for(int i = 0; conv->width < 0 || i < conv->width; i++) {
        char c = _next_char(state);
        if(isspace(c)) {
            _unwind_once(state);
            break;
        }

        if(!conv->assignment_suppressed)
            *dst++ = c;
    }

    if(!conv->assignment_suppressed) {
        state->num_assigned++;
        *dst = '\0';
    }
}

static void _bytes_read_conversion(struct conversion *conv, struct scanf_state *state) {
    _assign_int(conv, state, state->bytes_read);

    if(!conv->assignment_suppressed)
        state->num_assigned--; // don't count %n
}

static void _format(const char *restrict *format, struct scanf_state *state) {
    struct conversion conv = {0};
    conv.width = -1;

    if(**format == '*') {
        (*format)++;
        conv.assignment_suppressed = true;
    }

    _parse_width(&conv, format);
    _parse_length_modifier(&conv.length_modifier, format);

    conv.specifier = (*format)++[0];

    switch(conv.specifier) {
        case 'c':
            _char_conversion(&conv, state);
            break;
        case 's':
            _string_conversion(&conv, state);
            break;
        case '[':
            break;
        case 'd':
        case 'i':
            break;
        case 'u':
        case 'o':
        case 'x':
        case 'X':
            break;
        case 'a':
        case 'A':
        case 'e':
        case 'E':
        case 'f':
        case 'F':
        case 'g':
        case 'G':
            break;
        case 'p':
            break;
        case 'n':
            _bytes_read_conversion(&conv, state);
            break;
        case FMT_CHAR:
            _expect_char(state, FMT_CHAR);
    }
}

int scanf_impl(fmtscanner_t scanner, void *restrict userp, const char *restrict format, va_list ap) {
    struct scanf_state state = {0};
    state.scanner = scanner;
    state.userp = userp;
    va_copy(state.ap, ap);

    while(*format && !state.errored) {
        char *escape = strchrnul(format, FMT_CHAR);
        _scan(format, escape - format, &state);
        format += escape - format;

        if(!*format)
            break;

        format++;
        _format(&format, &state);
    }

    return state.errored ? EOF : (int) state.num_assigned;
}
