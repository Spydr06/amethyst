#include <geode.h>

#include <libshard.h>
#include "../shard.h"
#include "context.h"

#include <getopt.h>
#include <setjmp.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define errorf(fmt, ...) (fprintf(stderr, C_RED C_BLD "[error]" C_NOBLD " " fmt C_RST __VA_OPT__(,) __VA_ARGS__))

static const struct option cmdline_options[] = {
    {"help", 0, NULL, 'h'},
    {NULL,   0, NULL, 0  }
};

_Noreturn static void help(const char* progname) {
    printf("Usage: %s <input file> [OPTIONS]\n\n", progname);
    printf("Options:\n");
    printf("  -h, --help        Print this help text and exit.\n");
    exit(EXIT_SUCCESS);
}

static jmp_buf err_recovery;

__attribute__((noreturn)) 
static void error_handler(struct geode_context* ctx, struct geode_error err) {
    switch(err.type) {
        case GEODE_ERR_LIBSHARD_INIT:
            errorf("Failed initializing libshard: %s\n", strerror(err.payload.err_no));
            break;
        case GEODE_ERR_SHARD: {
            size_t num = err.payload.errs.count;
            errorf("Encountered %zu shard error%s:\n", num, num == 1 ? "" : "s");

            for(size_t i = 0; i < num; i++)
                geode_print_shard_error(err.payload.errs.items + i);
        } break;
        case GEODE_ERR_UNRECOGNIZED_ARG:
            errorf("Unrecognized argument `%s`.\nTry `%s --help` for more information.\n", err.payload.arg, ctx->prog_name);
            break;
    }

    longjmp(err_recovery, err.type); 
}

int main(int argc, char** argv) {
    int ret = EXIT_SUCCESS;
    if(setjmp(err_recovery)) {
        // exit gracefully
        ret = EXIT_FAILURE;
        goto finish;
    }

    struct geode_context ctx;
    geode_context_init(&ctx, argv[0], error_handler);

    int ch, long_index;
    while((ch = getopt_long(argc, argv, "h", cmdline_options, &long_index)) != EOF) {
        switch(ch) {
            case 'h':
                help(argv[0]);
            case '?':
            default:
                fprintf(stderr, "Try `%s --help` for more information.\n", argv[0]);
                ret = EXIT_FAILURE;
                goto finish;
        }
    }

    if(optind < argc)
        geode_throw(&ctx, UNRECOGNIZED_ARG, .arg=argv[optind]);

finish:
    geode_context_free(&ctx);
    
    return ret;
}

