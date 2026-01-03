#ifndef _SHARD_NO_REPL
#define _XOPEN_SOURCE 700
#include "shard.h"

#include <assert.h>
#include <ctype.h>
#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef _SHARD_NO_LIBEDIT
#include <histedit.h>
#endif

#define _LIBSHARD_INTERNAL
#define _AMETHYST_STRNLEN_DEFINED
#include <libshard.h>
#include <libshard-internal.h>

#include "shard_libc_driver.h"

#define REPL_HISTORY_SIZE 1024
#define DEFAULT_LINEBUFFER_SIZE 1024
#define COMMAND_PREFIX ':'

struct repl {
    struct shard_context* ctx;
    bool running;
    bool echo_result;

    unsigned current_line;
    unsigned line_offset;

    struct shard_source source;
    struct shard_string_list lines;
};

static int handle_list(struct repl* repl, char* args);
// static int handle_with(struct repl* repl, char* args);
static int handle_quit(struct repl* repl, char* args);
static int handle_help(struct repl* repl, char* args);

static struct {
    const char* cmd_short;
    const char* cmd_long;
    int (*handler)(struct repl*, char*);
    const char* description;
} commands[] = {
    // dummy commands (for :help)
    { "<expr>", NULL, NULL,        "Evaluate and print expression" },
    { "<x> := <expr>", NULL, NULL, "Bind expression to variable" },
    { "", NULL, NULL, "" },

    // commands
    { ":l", ":list", handle_list, "List the current inputs"},
//    { ":w", ":with", handle_with, "Pull an attr set into the current scope" },
    { ":q", ":quit", handle_quit, "Exit the shard repl" },
    { ":?", ":help", handle_help, "Shows this help text" }
};

static int repl_close_source(struct shard_source*) {
    return 0;
}

static int repl_read_all(struct shard_source* self, struct shard_string* dest) {
    struct repl* repl = self->userp;
    
    assert(repl->lines.count > repl->current_line);

    char* line = repl->lines.items[repl->current_line];
    dest->items = line;
    dest->capacity = dest->count = strlen(line) + 1;

    return 0;
}

static void repl_init(struct repl* repl, struct shard_context* ctx, bool echo_result) {
    memset(repl, 0, sizeof(struct repl));
    repl->running = true;
    repl->echo_result = echo_result;
    repl->ctx = ctx;

    repl->source = (struct shard_source) {
        .userp = repl,
        .line_offset = 1,
        .origin = REPL_ORIGIN,
        .close = repl_close_source,
        .read_all = repl_read_all,
    };
}

static void repl_free(struct repl* repl) {
    for(size_t i = 0; i < repl->lines.count; i++)
        free(repl->lines.items[i]);
    dynarr_free(repl->ctx, &repl->lines);
}

static __attribute__((format(printf, 1, 2)))
void errorf(const char* fmt, ...) {
    printf(C_BLD C_RED "error:" C_RST " ");

    va_list ap;
    va_start(ap, fmt);
    vprintf(fmt, ap);
    va_end(ap);

    printf("\n");
}

static int handle_command(struct repl* repl, char** line_ptr) {
    char* pch = strtok(*line_ptr, " \t\v\r\n");

    for(size_t i = 0; i < LEN(commands); i++) {
        if(strcmp(commands[i].cmd_short, pch) == 0 || (commands[i].cmd_long && strcmp(commands[i].cmd_long, pch) == 0))
            return commands[i].handler(repl, pch);
    }

    errorf("unknown command " C_BLD C_PURPLE "`%s`" C_RST, pch);
    return 0;
}

static int handle_list(struct repl* repl, char*) {
    for(size_t i = 0; i < repl->lines.count; i++) {
        printf(C_BLD C_BLACK " %4zu " C_NOBLD "| " C_RST "%s\n", i + 1, repl->lines.items[i]);
    }

    if(repl->lines.count == 0) {
        printf("<no list entries yet...>\n");
    }

    printf("\n");
    return 0;
}

static int handle_quit(struct repl* repl, char*) {
    repl->running = false;
    return EXIT_SUCCESS;
}

static int handle_help(struct repl*, char*) {
    printf("The following commands are available:\n\n");

    char buf[20];
    for(size_t i = 0; i < LEN(commands); i++) {
        memset(buf, 0, sizeof(buf));
        strncat(buf, commands[i].cmd_short, LEN(buf) - 1);
        if(commands[i].cmd_long) {
            strncat(buf, ", ", LEN(buf) - 1);
            strncat(buf, commands[i].cmd_long, LEN(buf) - 1);
        }

        for(size_t i = strlen(buf); i < LEN(buf); i++)
            buf[i] = ' ';

        // TODO: fixme
        // printf("%.*s%s\n", (int) LEN(buf), buf, commands[i].description);
        printf("%.20s%s\n", /*(int) LEN(buf), */buf, commands[i].description);
    }

    printf("\n");

    return EXIT_SUCCESS;
}

static void trim_input(char** input) {
    while(isspace(**input)) (*input)++;
    size_t len = strlen(*input);
    if(len == 0)
        return;

    size_t last = len - 1;
    while(isspace((*input)[last])) (*input)[last--] = '\0';
}

static void push_line(struct repl* repl, char* line) {
    repl->current_line = repl->lines.count;
    repl->line_offset = 0;
    dynarr_append(repl->ctx, &repl->lines, line);
}

static char* pop_line(struct repl* repl) {
    assert(repl->lines.count > 0);

    repl->current_line = repl->lines.count - 1;
    repl->line_offset = 0;
    return repl->lines.items[--repl->lines.count];
}

static void print_repl_error(struct repl* repl, struct shard_error* error) {
    char* line = repl->lines.items[error->loc.line - 1];

    fprintf(stderr,
        C_BLD C_RED "error:" C_RST " %s\n " C_BLD C_BLUE "       at " C_PURPLE "<repl>:%u:\n" C_RST,
        EITHER(error->err, strerror(error->_errno)),
        error->loc.line
    );

    fprintf(stderr,
        C_BLD C_BLACK " %4d " C_NOBLD "| " C_RST "%s",
        error->loc.line,
        line
    );

    if(line[strlen(line) - 1] != '\n')
        fprintf(stderr, "\n");

    int column = error->loc.offset;

    fprintf(stderr, C_BLACK "      | " C_RST "%*s" C_RED, column, "");

    for(size_t i = 0; i < MAX(error->loc.width, 1); i++)
        fputc('^', stderr);
    fprintf(stderr, C_RST "\n");
}

static void emit_errors(struct repl* repl) {
    struct shard_error* errors = shard_get_errors(repl->ctx);
    for(size_t i = 0; i < shard_get_num_errors(repl->ctx); i++) {
        struct shard_error* err = &errors[i];
        if(err->loc.src->origin == repl->source.origin)
            print_repl_error(repl, err);
        else
            print_shard_error(stderr, err);
    }

    shard_remove_errors(repl->ctx);
}

static int repl_eval(struct repl* repl, char* line, struct shard_value* value, bool echo_result) {
    push_line(repl, line);

    repl->source.line_offset = repl->current_line + 1;
    struct shard_open_source open_source = {
        .source = repl->source,
        .opened = true,
        .auto_close = false,
        .auto_free = false,
        .evaluated = false,
        .parsed = false,
    };
    
    int num_errors = shard_eval(repl->ctx, &open_source);
    if(!num_errors && echo_result) {

        struct shard_string str = {0};
        num_errors = shard_value_to_string(repl->ctx, &str, &open_source.result, 1);

        if(!num_errors) {
            shard_string_push(repl->ctx, &str, '\0');
            puts(str.items);
        }

        shard_string_free(repl->ctx, &str);
    }

    if(!num_errors) {
        memcpy(value, &open_source.result, sizeof(struct shard_value));
        return EXIT_SUCCESS;
    }

    emit_errors(repl);
    assert(pop_line(repl) == line);

    free(line);

    return EXIT_FAILURE;
}

static int handle_line(struct repl* repl, char* line) {
    trim_input(&line);

    if(line[0] == COMMAND_PREFIX) {
        int ret = handle_command(repl, &line);
        free(line);
        return ret;
    }
    
    if(strlen(line) == 0)
        return 0;

    char* bind_operator = strstr(line, ":=");
    char* expr_begin = line;
    if(bind_operator)
        expr_begin = bind_operator + 2;     
    
    struct shard_value result;
    int ret = repl_eval(repl, strdup(expr_begin), &result, bind_operator ? false : true);
    if(ret) {
        free(line);
        return ret;
    }

    if(bind_operator) {
        if(bind_operator == line) {
            errorf("syntax error: `:=` expects identifier on left side");
            ret = EXIT_FAILURE;
            goto cleanup;    
        }

        bind_operator[-1] = '\0';
        char* ident = line;
        trim_input(&ident);
        size_t ident_len = strlen(ident);

        for(size_t i = 0; i < ident_len; i++) {
            if(isalnum(ident[i]) || ident[i] == '-' || ident[i] == '_' || ident[i] == '.')
                continue;

            errorf("syntax error: `:=` expects left operator to be an identifier");
            ret = EXIT_FAILURE;
            goto cleanup;
        }

        int err = shard_define_constant(repl->ctx, shard_get_ident(repl->ctx, ident), result);
        if(err) {
            ret = EXIT_FAILURE;
            errorf("failed binding to `%s`: %s", ident, strerror(err));
            goto cleanup;
        }
    }

cleanup:
    free(line);

    return EXIT_SUCCESS;
}

#ifdef _SHARD_NO_LIBEDIT

static char* repl_prompt(void) {
    return "shard> ";
}

static char* multiline_prompt(void) {
    return "> ";
}

static char* empty_prompt(void) {
    return "";
}

#else

static char* repl_prompt(EditLine* el) {
    (void) el;
    return "shard> ";
}

static char* multiline_prompt(EditLine* el) {
    (void) el;
    return "> ";
}

static char* empty_propt(EditLine* el) {
    (void) el;
    return "";
}

#endif

static void print_version(void) {
    printf("Shard " SHARD_VERSION " (" __DATE__ ", " __TIME__ ")\n");
    printf("Type `:help` or `:?` for more information. Use `:q` to quit.\n");
}

int shard_repl(const char* progname, struct shard_context* ctx, bool echo_result) {
    if(echo_result)
        print_version();

    struct repl repl;
    repl_init(&repl, ctx, echo_result);

#ifdef _SHARD_NO_LIBEDIT
    char* (*prompt)(void) = repl_prompt;
#else
    EditLine* el = el_init(progname, stdin, stdout, stderr);
    if(!el)
        return EXIT_FAILURE;

    History* hist = history_init();
    if(!hist) {
        el_end(el);
        fprintf(stderr, "failed inititializing history: %s\n", strerror(errno));
        return EXIT_FAILURE;
    }

    HistEvent hist_ev;
    history(hist, &hist_ev, H_SETSIZE, REPL_HISTORY_SIZE);

    el_set(el, EL_PROMPT, empty_propt);
    el_set(el, EL_HIST, history, hist);
    el_set(el, EL_SAFEREAD, 1);
#endif 

    int ret = 0;

    int count;

    bool multiline = false;
    int line_buffer_size = DEFAULT_LINEBUFFER_SIZE;
    int line_buffer_count;
    char* line_buffer = malloc(line_buffer_size + 1);

#ifdef _SHARD_NO_LIBEDIT
    // TODO: support longer lines
    char line[1024];

    while(repl.running) {
        printf("%s", prompt());

        if(fflush(stdout) || fgets(line, sizeof(line), stdin) == NULL)
            break;

        count = (int) strnlen(line, sizeof(line));
#else
    const char* line;

    while(repl.running && (line = el_gets(el, &count)) != NULL) {
#endif
        if(!multiline)
            line_buffer_count = 0;

        if(count > line_buffer_size) {
            line_buffer_size = count;
            line_buffer = realloc(line_buffer, line_buffer_size + 1);
        }

        memcpy(line_buffer + line_buffer_count, line, count + 1);
        line_buffer_count += strlen(line);

        if(line_buffer_count > 1 && line_buffer[line_buffer_count - 2] == '\\') {
            line_buffer[line_buffer_count -= 2] = '\n';
            multiline = true;
#ifdef _SHARD_NO_LIBEDIT
            prompt = multiline_prompt;
#else
            el_set(el, EL_PROMPT, multiline_prompt);
#endif
            continue;
        }

        multiline = false;
#ifdef _SHARD_NO_LIBEDIT
        prompt = repl_prompt;
#else
        el_set(el, EL_PROMPT, repl_prompt);
#endif
    
        handle_line(&repl, strdup(line_buffer));

#ifndef _SHARD_NO_LIBEDIT
        if((line_buffer_count = strlen(line_buffer)))
           history(hist, &hist_ev, H_ENTER, line_buffer);
#endif

        fflush(stderr);
    }

    free(line_buffer);

#ifndef _SHARD_NO_LIBEDIT
    history_end(hist);
    el_end(el);
#endif

    repl_free(&repl);

    return ret;
}

#endif /* _SHARD_NO_REPL */

