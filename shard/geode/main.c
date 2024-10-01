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

typedef int (*action_handler_t)(struct geode_context* ctx);

struct action {
    const char* name;
    const char* desc;
    action_handler_t handler;
};

static const struct action cmdline_actions[] = {
    {NULL, NULL, NULL}
};

static const struct option cmdline_options[] = {
    {"help",   0,                 NULL, 'h'},
    {"prefix", required_argument, NULL, 'p'},
    {"config", required_argument, NULL, 'c'},
    {NULL,     0,                 NULL, 0  }
};

static void help(struct geode_context* ctx) {
    printf("Usage: %s <input file> [ACTIONS] [OPTIONS]\n", ctx->prog_name);
    printf("\nActions:\n");

    const struct action* action = cmdline_actions;
    while(action->name) {
        printf("  %s\t\t%s\n", action->name, action->desc);
        action++;
    }

    printf("\nOptions:\n");
    printf("  -h, --help        Print this help text and exit.\n");
    printf("  -p, --prefix      Set the system prefix. [%s]\n", ctx->prefix);
    printf("  -c, --config      Set the system configuration path. [%s]\n", ctx->main_config_path);
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
        case GEODE_ERR_UNRECOGNIZED_ACTION:
            errorf("Unrecognized action `%s`.\nTry `%s --help` for more information.\n", err.payload.action, ctx->prog_name);
            break;
        case GEODE_ERR_NO_ACTION:
            errorf("No action specified.\nTry `%s --help` for more information.\n", ctx->prog_name);
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
    while((ch = getopt_long(argc, argv, "p:c:h", cmdline_options, &long_index)) != EOF) {
        switch(ch) {
            case 'p':
                geode_context_set_prefix(&ctx, optarg);
                break;
            case 'c':
                geode_context_set_config_file(&ctx, optarg);
                break;
            case 'h':
                help(&ctx);
                goto finish;
            case '?':
            default:
                fprintf(stderr, "Try `%s --help` for more information.\n", argv[0]);
                ret = EXIT_FAILURE;
                goto finish;
        }
    }

    if(optind >= argc)
        geode_throw(&ctx, NO_ACTION, );

    for(; optind < argc; optind++) {
        const struct action* action = cmdline_actions;
        bool found = false;
        
        while(action->name) {
            if(strcmp(action->name, argv[optind]) == 0) {
                found = true;
                break;
            }
            action++;
        }

        if(!found)
            geode_throw(&ctx, UNRECOGNIZED_ACTION, .action=argv[optind]);

        ret = action->handler(&ctx);
        if(ret)
            goto finish;
    }

finish:
    geode_context_free(&ctx);
    
    return ret;
}

