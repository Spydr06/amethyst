#include <geode.h>

#include <libshard.h>

#include <context.h>
#include <bootstrap.h>

#include <getopt.h>
#include <setjmp.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef int (*action_handler_t)(struct geode_context* ctx, int argc, char** argv);

struct action {
    const char* name;
    const char* desc;
    action_handler_t handler;
};

static const struct action cmdline_actions[] = {
    {"bootstrap", "Bootstrap a new root file system from a configuration file", geode_bootstrap},
    {NULL, NULL, NULL}
};

static const struct option cmdline_options[] = {
    {"help",    no_argument,       NULL, 'h'},
    {"prefix",  required_argument, NULL, 'p'},
    {"config",  required_argument, NULL, 'c'},
    {"verbose", no_argument,       NULL, 'v'},
    {NULL,      0,                 NULL, 0  }
};

static void help(struct geode_context* ctx) {
    printf("Usage: %s [ACTION] [<ARGUMENTS>] [<OPTIONS>]\n", ctx->prog_name);
    printf("\nActions:\n");

    const struct action* action = cmdline_actions;
    while(action->name) {
        printf("  %s\t\t%s\n", action->name, action->desc);
        action++;
    }

    printf("\nOptions:\n");
    printf("  -h, --help              Print this help text and exit.\n");
    printf("  -p, --prefix <path>     Set the system prefix. [%s]\n", ctx->prefix);
    printf("  -c, --config <path>     Set the system configuration path. [%s]\n", ctx->main_config_path);
    printf("  -v, --verbose           Enable verbose output.\n");

    printf("\n");
}

static jmp_buf err_recovery;

__attribute__((noreturn)) 
static void error_handler(struct geode_context* ctx, struct geode_error err) {
    switch(err.type) {
        case GEODE_ERR_LIBSHARD_INIT:
            errorf("Failed initializing libshard: %s\n", strerror(err.payload.err_no));
            break;
        case GEODE_ERR_SHARD: {
            int num = err.payload.shard.num;
            errorf("Encountered %d shard error%s:\n", num, num == 1 ? "" : "s");

            for(int i = 0; i < num; i++)
                geode_print_shard_error(err.payload.shard.errs + i);
        } break;
        case GEODE_ERR_UNRECOGNIZED_ACTION:
            errorf("Unrecognized action `%s`.\nTry `%s --help` for more information.\n", err.payload.action, ctx->prog_name);
            break;
        case GEODE_ERR_NO_ACTION:
            errorf("No action specified.\nTry `%s --help` for more information.\n", ctx->prog_name);
            break;
        case GEODE_ERR_FILE_IO:
            errorf("Could not access file `%s`: %s\n", err.payload.file.path, strerror(err.payload.file.err_no));
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
    while((ch = getopt_long(argc, argv, "vp:c:h", cmdline_options, &long_index)) != EOF) {
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
            case 'v':
                ctx.flags.verbose = true;
                break;
            case '?':
            default:
                fprintf(stderr, "Try `%s --help` for more information.\n", argv[0]);
                ret = EXIT_FAILURE;
                goto finish;
        }
    }

    if(optind >= argc)
        geode_throw(&ctx, NO_ACTION, );

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

    ret = action->handler(&ctx, argc - (optind + 1), argv + (optind + 1));
    if(ret)
        goto finish;

finish:
    geode_context_free(&ctx);
    
    return ret;
}

