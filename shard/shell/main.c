#define _AMETHYST_SOURCE

#include <getopt.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdnoreturn.h>
#include <string.h>

#include <libshard.h>

#include "eval.h"
#include "shell.h"
#include "resource.h"

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
    printf("shard-sh, version " SHARD_SHELL_VERSION " (using libshard version %s)\n", SHARD_VERSION);
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
    fprintf(stdout, fmt, '\n');
    fflush(stdout);
}

__attribute__((format(printf, 1, 2))) void errorf(const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    verrorf(fmt, ap);
    va_end(ap);
}

void shell_load_defaults(void) {
    shell.prompt = "$ ";
    shell.history_size = DEFAULT_HISTORY_SIZE;
}

void shell_cleanup(void) {

}

int main(int argc, char** argv) {
    shell_load_defaults();
    shell.progname = argv[0];
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

        struct shell_resource resource;
        if((err = resource_from_string(eval_str, false, &resource))) {
            errorf("%s", strerror(err));
            exit(EXIT_FAILURE);
        }

        struct shell_state state;
        shell_state_init(&state);

        err = shell_eval(&state, &resource);

        shell_state_free(&state);

        resource_free(&resource);

        exit(err ? EXIT_FAILURE : EXIT_SUCCESS);
    }

    for(; optind < argc; optind++) {
        repl = false;
        char* filepath = argv[optind];

        struct shell_resource resource;
        if((err = resource_from_file(filepath, "r", &resource))) {
            errorf("%s: %s", filepath, strerror(err));
            exit(EXIT_FAILURE);
        }

        struct shell_state state;
        shell_state_init(&state);

        err = shell_eval(&state, &resource);
        if(err && shell.exit_on_error)
            exit(EXIT_FAILURE);

        shell_state_free(&state);

        resource_free(&resource);
    }

    if(repl && (err = shell_repl()))
        return EXIT_FAILURE;

    return EXIT_SUCCESS;
}

