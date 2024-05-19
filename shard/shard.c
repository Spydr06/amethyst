#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <getopt.h>

#include <libshard.h>

#define _(str) str

static const struct option cmdline_options[] = {
    {"help", 0, NULL, 'h'},
    {NULL,   0, NULL, 0  }
};

static void _Noreturn help(const char* progname)
{
    printf("Usage: %s <input file> [OPTIONS]\n\n", progname);
    printf("Options:\n");
    printf("  -h, --help        Print this help text and exit.\n");
    exit(EXIT_SUCCESS);
}

int main(int argc, char** argv) {
    int ch, long_index;
    while((ch = getopt_long(argc, argv, "h", cmdline_options, &long_index)) != EOF) {
        switch(ch) {
        case 0:
            if(strcmp(cmdline_options[long_index].name, "help") == 0)
                help(argv[0]);
            break;
        case 'h':
            help(argv[0]);
            break;
        case '?':
            fprintf(stderr, "%s: unrecognized option -- %s\n", argv[0], argv[optind]);
            fprintf(stderr, "Try `%s --help` for more information.\n", argv[0]);
            exit(EXIT_FAILURE);
            break;
        default:
            fprintf(stderr, "%s: invalid option -- %c\n", argv[0], ch);
            fprintf(stderr, "Try `%s --help` for more information.\n", argv[0]);
            exit(EXIT_FAILURE);
        }
    }

    if(optind >= argc) {
        fprintf(stderr, "%s: no input files provided.\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    struct shard_context ctx = {
        .malloc = malloc,
        .realloc = realloc,
        .free = free,
    };

    int err = shard_init(&ctx);
    if(err) {
        fprintf(stderr, "%s: error initializing libshard: %s\n", argv[0], strerror(err));
        exit(EXIT_FAILURE);
    }

    for(; optind < argc; optind++) {
        const char* input_file = argv[optind];
        FILE* fd = fopen(input_file, "r");
        if(!fd) {
            fprintf(stderr, "%s: could not read file `%s`: %s\n", argv[0], input_file, strerror(errno));
            goto finish;
        }

        struct shard_source src = {
            .type = SHARD_FILE,
            .file = fd
        };
        
        int num_errors = shard_run(&ctx, &src);
        struct shard_error* errors = shard_get_errors(&ctx);
        for(int i = 0; i < num_errors; i++)
            shard_print_error(&errors[i], stderr);

        if(num_errors > 0)
            exit(EXIT_FAILURE);

        fclose(fd);
    }

finish:
    shard_deinit(&ctx);

    return 0;
}
