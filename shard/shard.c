#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <getopt.h>
#include <assert.h>

#include <libshard.h>

#define EITHER(a, b) ((a) ? (a) : (b))
#define MAX(a, b) ((a) > (b) ? (a) : (b))

#define C_RED "\033[31m"
#define C_BLACK "\033[90m"

#define C_BLD "\033[1m"
#define C_RST "\033[0m"
#define C_NOBLD "\033[22m"

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
    FILE* fd = error->loc.src->userp;
    assert(fd);

    size_t fd_pos = ftell(fd), line_start = error->loc.offset;

    while(line_start > 0) {
        fseek(fd, --line_start - 1, SEEK_SET);
        if(fgetc(fd) == '\n')
            break;
    }

    size_t line_end = error->loc.offset + error->loc.width - 1;
    fseek(fd, line_end, SEEK_SET);

    char c;
    while((c = fgetc(fd)) != '\n' && c != EOF)
        line_end++;

    fseek(fd, line_start, SEEK_SET);

    char* line_str = calloc(line_end - line_start + 1, sizeof(char));
    fread(line_str, line_end - line_start, sizeof(char), fd);

    size_t column = error->loc.offset - line_start;

    fprintf(stderr, 
        C_BLD C_RED "[Error]" C_RST C_BLD " %s:%u:%zu:" C_NOBLD " %s\n" C_RST,
        error->loc.src->origin, error->loc.line, column, EITHER(error->err, strerror(error->_errno))
    );

    fprintf(stderr,
        C_BLD C_BLACK " %4d " C_NOBLD "| " C_RST,
        error->loc.line
    );

    fwrite(line_str, sizeof(char), column, stderr);
    fprintf(stderr, C_RED C_RST);
    fwrite(line_str + column, sizeof(char), error->loc.width, stderr);
    fprintf(stderr, C_RST "%s\n", line_str + column + error->loc.width);

    fprintf(stderr, C_BLACK "      |" C_RST " %*s" C_RED, (int) column, "");

    for(size_t i = 0; i < MAX(error->loc.width, 1); i++)
        fputc('^', stderr);
    fprintf(stderr, C_RST "\n");

    fseek(fd, fd_pos, SEEK_SET);
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
