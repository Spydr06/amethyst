#define _XOPEN_SOURCE 700
#include "shard.h"

#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#include <string.h>
#include <stdarg.h>

#define __unused __attribute__((unused))

static int handle_quit(struct shard_context* ctx, char* args, bool* running);
static int handle_help(struct shard_context* ctx, char* args, bool* running);

static struct {
    const char* cmd_short;
    const char* cmd_long;
    int (*handler)(struct shard_context* ctx, char* args, bool* running);
    const char* description;
} commands[] = {
    // dummy commands (for :help)
    { "<expr>", NULL, NULL,       "Evaluate and print expression" },
    { "<x> = <expr>", NULL, NULL, "Bind expression to variable" },

    // commands
    { ":q", ":quit", handle_quit, "Exit the shard repl" },
    { ":?", ":help", handle_help, "Shows this help text" }
};

static __attribute__((format(printf, 1, 2)))
void errorf(const char* fmt, ...) {
    printf(C_BLD C_RED "error:" C_RST " ");

    va_list ap;
    va_start(ap, fmt);
    vprintf(fmt, ap);
    va_end(ap);

    printf("\n");
}

static int handle_command(struct shard_context* ctx, char** line_ptr, bool* running) {
    char* pch = strtok(*line_ptr, " \t\v\r\n");

    for(size_t i = 0; i < LEN(commands); i++) {
        if(strcmp(commands[i].cmd_short, pch) == 0 || (commands[i].cmd_long && strcmp(commands[i].cmd_long, pch) == 0))
            return commands[i].handler(ctx, pch, running);
    }

    errorf("unknown command " C_BLD C_PURPLE "`%s`" C_RST, pch);
    return 0;
}

static int handle_quit(struct shard_context* __unused, char* __unused, bool* running) {
    *running = false;
    return EXIT_SUCCESS;
}

static int handle_help(struct shard_context* __unused, char* __unused, bool* __unused) {
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

        printf("%.*s%s\n", (int) LEN(buf), buf, commands[i].description);
    }
    return EXIT_SUCCESS;
}

static int handle_line(struct shard_context* ctx, bool echo_result, char* line_ptr, bool* running) {
    if(*line_ptr == ':')
        return handle_command(ctx, &line_ptr, running);
    
    if(strlen(line_ptr) > 0) {
        printf("TODO: eval `%s`", line_ptr);
    }
    return 0;
}

int shard_repl(struct shard_context* ctx, bool echo_result) {
    bool running = true;
    int ret = 0;

    while(running) {
        printf("shard-repl> ");
        fflush(stdout);

        char* line = NULL;
        size_t len;
        size_t read = getline(&line, &len, stdin);

        char* line_ptr = line;
        while(isspace(*line_ptr)) line_ptr++;

        if(strlen(line) > 0 && handle_line(ctx, echo_result, line_ptr, &running)) {
            ret = EXIT_FAILURE;
            running = false;
        }

        free(line);

        printf("\n");
    }

    return ret;
}

