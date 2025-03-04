#define _AMETHYST_SOURCE

#include <stdlib.h>
#include <stdio.h>
#include <getopt.h>
#include <stdarg.h>
#include <stdnoreturn.h>

#include <libshard.h>

static noreturn void help(void) {
    printf("Usage: %s [<input file>] [OPTIONS]\n\n", _getprogname());
    printf("Options:\n");
    printf("  -h        Print this help text and exit.\n");
    printf("  -c <expr> Evaluate a given expression.\n");
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

noreturn void verrorf(const char *fmt, va_list ap) {
    fprintf(stdout, "%s: ", _getprogname());
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

int main(int argc, char** argv) {
    bool verbose = false;
    char* eval_str = NULL;

    int c;
    while((c = getopt(argc, argv, "c:hvV")) != EOF) {
        switch(c) {
            case 'h':
                help();
            case 'v':
                verbose = true;
                break;
            case 'V':
                version();
            case 'c':
                eval_str = optarg;
                break;

            case '?':
            default:
                errorf("invalid option -- '%c'\nTry `%s --help` for more information.", c, _getprogname());
                exit(1);
        }
    }

    bool repl = !eval_str;

    for(; optind < argc; optind++) {
        repl = false;
        char* filename = argv[optind];
    }

    if(!repl)
        return EXIT_SUCCESS;

    printf("Shell!\n");
    while(1);
}
