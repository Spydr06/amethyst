#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <getopt.h>

#include <libshard.h>

#define _(str) str

#define C_RED "\033[31m"
#define C_BLACK "\033[37m"

#define C_BLD "\033[1m"
#define C_RST "\033[0m"

static const struct option cmdline_options[] = {
    {"help", 0, NULL, 'h'},
    {NULL,   0, NULL, 0  }
};

_Noreturn static void help(const char* progname)
{
    printf("Usage: %s <input file> [OPTIONS]\n\n", progname);
    printf("Options:\n");
    printf("  -h, --help        Print this help text and exit.\n");
    exit(EXIT_SUCCESS);
}

static int _getc(struct shard_source* src) {
    return fgetc(src->userp);
}

static int _ungetc(int c, struct shard_source* src) {
    return ungetc(c, src->userp);
}

static int _tell(struct shard_source* src) {
    return ftell(src->userp);
}

void print_error(struct shard_error* error) {
    fprintf(
        stderr,
        C_RED C_BLD "[Error]" C_RST " %s:%d:%d: %s\n",
        error->loc.src->origin, error->loc.line, error->loc.offset + 1,
        error->err
    );
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

    int ret = EXIT_SUCCESS;
    for(; optind < argc; optind++) {
        const char* input_file = argv[optind];
        FILE* fd = fopen(input_file, "r");
        if(!fd) {
            fprintf(stderr, "%s: could not read file `%s`: %s\n", argv[0], input_file, strerror(errno));
            ret = EXIT_FAILURE;
            goto finish;
        }

        struct shard_source src = {
            .userp = fd,
            .origin = input_file,
            .getc = _getc,
            .ungetc = _ungetc,
            .tell = _tell,
            .line = 1
        };
        
        struct shard_value result;
        int num_errors = shard_eval(&ctx, &src, &result);
        struct shard_error* errors = shard_get_errors(&ctx);
        for(int i = 0; i < num_errors; i++)
            print_error(&errors[i]);

        fclose(fd);

        if(num_errors > 0) {
            ret = EXIT_FAILURE;
            goto finish;
        }
    }

finish:
    shard_deinit(&ctx);
    if(ret)
        fputs("execution terminated.\n", stderr);

    return ret;
}
