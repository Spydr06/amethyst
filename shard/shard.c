#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>

#include <getopt.h>
#include <libshard.h>

#include "shard_libc_driver.h"
#include "shard.h"

static const struct option cmdline_options[] = {
    {"help",   0, NULL, 'h'},
    {"silent", 0, NULL, 's'},
    {"system", 1, NULL, 0  },
    {NULL,     0, NULL, 0  }
};

_Noreturn static void help(const char* progname)
{
    printf("Usage: %s <input file> [OPTIONS]\n\n", progname);
    printf("Options:\n");
    printf("  -h, --help        Print this help text and exit.\n");
    printf("  -I <include path> Add a directory to the include path list.\n");
    printf("  -s, --silent      Disable printing the resulting value.\n");
    exit(EXIT_SUCCESS);
}

static void emit_errors(struct shard_context* ctx) {
    struct shard_error* errors = shard_get_errors(ctx);
    for(size_t i = 0; i < shard_get_num_errors(ctx); i++)
        print_shard_error(stderr, &errors[i]);

    shard_remove_errors(ctx);
}

static int eval_file(struct shard_context* ctx, const char* progname, const char* input_file, bool echo_result) { 
    struct shard_open_source* source = shard_open(ctx, input_file);
    if(!source) {
        fprintf(stderr, "%s: could not read file `%s`: %s\n", progname, input_file, strerror(errno));
        return EXIT_FAILURE;
    }
    
    int num_errors = shard_eval(ctx, source);

    if(!num_errors && echo_result) {
        struct shard_string str = {0};
        num_errors = shard_value_to_string(ctx, &str, &source->result, 1);

        if(!num_errors) {
            shard_string_push(ctx, &str, '\0');
            puts(str.items);
        }

        shard_string_free(ctx, &str);
    }

    if(num_errors)
        emit_errors(ctx);

    return num_errors ? EXIT_FAILURE : EXIT_SUCCESS;
}

int main(int argc, char** argv) {
    struct shard_context ctx;
    shard_context_default(&ctx);

    int err = shard_init(&ctx);
    if(err) {
        fprintf(stderr, "%s: error initializing libshard: %s\n", argv[0], strerror(err));
        exit(EXIT_FAILURE);
    }
    
    const char* current_system = PLATFORM_STRING;
    bool echo_result = true;
    int ch, long_index;
    while((ch = getopt_long(argc, argv, "hI:s", cmdline_options, &long_index)) != EOF) {
        switch(ch) {
        case 0:
            if(strcmp(cmdline_options[long_index].name, "help") == 0)
                help(argv[0]);
            if(strcmp(cmdline_options[long_index].name, "system") == 0)
                current_system = optarg;
            break;
        case 'h':
            help(argv[0]);
            break;
        case 'I':
            shard_include_dir(&ctx, optarg);
            break;
        case 's':
            echo_result = false;
            break;
        case '?':
        default:
            fprintf(stderr, "Try `%s --help` for more information.\n", argv[0]);
            exit(EXIT_FAILURE);
        }
    }

    shard_set_current_system(&ctx, current_system);

    char* path = getenv("SHARD_PATH");
    if(path)
        shard_include_dir(&ctx, path);

    int ret = 0;
    if(optind >= argc) {
        ret = shard_repl(argv[0], &ctx, echo_result);
    }

    for(; optind < argc; optind++) {
        const char* input_file = argv[optind];
        ret = eval_file(&ctx, argv[0], input_file, echo_result);
        if(ret)
            break;
    }

    shard_deinit(&ctx);
    if(ret)
        fputs("execution terminated.\n", stderr);

    return ret;
}

