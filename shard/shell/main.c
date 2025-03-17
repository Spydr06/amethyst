#define _AMETHYST_SOURCE

#include <assert.h>
#include <getopt.h>
#include <libgen.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdnoreturn.h>
#include <string.h>
#include <unistd.h>

#include <libshard.h>
#include <libshard-util.h>

#include "../shard_libc_driver.h"

#include "eval.h"
#include "shell.h"

__attribute__((section(".data")))
struct shell shell;

static noreturn void help(void) {
    printf("Usage: %s [<input file>] [OPTIONS]\n\n", shell.progname);
    printf("Options:\n");
    printf("  -h        Print this help text and exit.\n");
    printf("  -c <expr> Evaluate a given expression.\n");
    printf("  -e        Exit on error.\n");
    printf("  -v        Verbose output.\n");
    printf("  -V        Print the shell version and exit.\n\n");
    exit(EXIT_SUCCESS);
}

static noreturn void version(void) {
    printf("%s, version " SHARD_SHELL_VERSION " (using libshard version %s)\n", shell.name, SHARD_VERSION);
    printf("Copyright (C) 2025 Spydr06\n");
    printf("License: MIT\n\n");
    printf("This is free software; see the source for copying conditions;\n");
    printf("you may redistribute it under the terms of the MIT license.\n");
    printf("This program has absolutely no warranty.\n\n");
    exit(EXIT_SUCCESS);
}

void verrorf(const char *fmt, va_list ap) {
    fprintf(stdout, "%s: ", shell.progname);
    vfprintf(stdout, fmt, ap);
    fprintf(stdout, "\n");
    fflush(stdout);
}

__attribute__((format(printf, 1, 2))) void errorf(const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    verrorf(fmt, ap);
    va_end(ap);
}

int print_shard_errors(void) {
    size_t num_errors = shard_get_num_errors(&shell.shard);
    struct shard_error* errors = shard_get_errors(&shell.shard);
    for(size_t i = 0; i < num_errors; i++) {
        print_shard_error(stderr, &errors[i]);
    }
    shard_remove_errors(&shell.shard);
    return (int) num_errors;
}

void shell_load_defaults(int argc, char** argv) {
    assert(argc > 0);
    shell.progname = argv[0];

    shell.progname_copy = strdup(shell.progname);
    shell.name = basename(shell.progname_copy);

    shell.prompt = "$ ";
    shell.history_size = DEFAULT_HISTORY_SIZE;

    shard_context_default(&shell.shard); 

    int err = shard_init(&shell.shard);
    if(err) {
        errorf("error initializing libshard: %s", strerror(err));
        exit(EXIT_FAILURE);
    }

    shard_hashmap_init(&shell.shard, &shell.aliases, 8);

    if((err = shell_load_builtins())) {
        errorf("failed defining shell builtins: %s", strerror(err));
        exit(EXIT_FAILURE);
    }

    if((err = shell_prelude())) {
        errorf("failed loading `prelude.shard`");
        exit(EXIT_FAILURE);
    }
}

void shell_cleanup(void) {
    shard_hashmap_free(&shell.shard, &shell.aliases);

    shard_deinit(&shell.shard);
    free(shell.progname_copy);
}

int main(int argc, char** argv) {
    shell_load_defaults(argc, argv);
    atexit(shell_cleanup); // defer cleanup

    char* eval_str = NULL;

    int err;
    int c;
    while((c = getopt(argc, argv, "ce:hvV")) != EOF) {
        switch(c) {
            case 'h':
                help();
            case 'v':
                shell.verbose_output = true;
                break;
            case 'V':
                version();
            case 'c':
                eval_str = optarg;
                break;
            case 'e':
                shell.exit_on_error = true;
                break;

            case '?':
            default:
                errorf("invalid option -- '%c'\nTry `%s --help` for more information.", c, shell.progname);
                exit(1);
        }
    }

    bool repl = !eval_str;

    if(eval_str) {
        if(optind < argc) {
            errorf("unexpected argument `%s`", argv[optind]);
            exit(EXIT_FAILURE);
        }

        struct shard_source source;
        int err = shard_string_source(&shell.shard, &source, "<inline string>", eval_str, strlen(eval_str) + 1, 0);
        if(err) {
            errorf("%s", strerror(err));
            exit(EXIT_FAILURE);
        }

        err = shell_eval(&source, 0);

        exit(err ? EXIT_FAILURE : EXIT_SUCCESS);
    }

    for(; optind < argc; optind++) {
        repl = false;
        char* filepath = argv[optind];

        struct shard_source source;
        int err = shard_open_callback(filepath, &source, "r");
        if(err) {
            errorf("%s: %s", filepath, strerror(err));
            exit(EXIT_FAILURE);
        }

        err = shell_eval(&source, 0);
        if(err && shell.exit_on_error)
            exit(EXIT_FAILURE);
    }

    if(repl && (err = shell_repl()))
        return EXIT_FAILURE;

    return EXIT_SUCCESS;
}

