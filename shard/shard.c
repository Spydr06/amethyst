#include "libshard.h"
#if __STDC_VERSION__ >= 199901L
#define _XOPEN_SOURCE 600
#else
#define _XOPEN_SOURCE 500
#endif /* __STDC_VERSION__ */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <assert.h>

#include <unistd.h>
#include <getopt.h>
#include <libgen.h>

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

static int _getc(struct shard_source* src) {
    return fgetc(src->userp);
}

static int _ungetc(int c, struct shard_source* src) {
    return ungetc(c, src->userp);
}

static int _tell(struct shard_source* src) {
    return ftell(src->userp);
}

static int _close(struct shard_source* src) {
    return fclose(src->userp);
}

static int _open(const char* path, struct shard_source* dest, const char* restrict mode) {
    FILE* fd = fopen(path, mode);
    if(!fd)
        return errno;

    *dest = (struct shard_source){
        .userp = fd,
        .origin = path,
        .getc = _getc,
        .ungetc = _ungetc,
//        .tell = _tell,
        .close = _close,
//        .line = 1
    };

    return 0;
}

static void print_basic_error(struct shard_error* error) {
    fprintf(stderr, 
        C_BLD C_RED "error: " C_RST C_BLD " %s:%u:" C_NOBLD " %s\n" C_RST,
        error->loc.src->origin, error->loc.line, EITHER(error->err, strerror(error->_errno))
    );
}

static void print_error(struct shard_error* error) {
    if(error->loc.src->getc != _getc) {
        print_basic_error(error);
        return;
    }

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
    while(((c = fgetc(fd)) != '\n') && c != EOF)
        line_end++;

    fseek(fd, line_start, SEEK_SET);

    char* line_str = calloc(line_end - line_start + 1, sizeof(char));
    fread(line_str, line_end - line_start, sizeof(char), fd);

    size_t column = error->loc.offset - line_start;

    fprintf(stderr, 
        C_BLD C_RED "error:" C_RST C_RST " %s\n " C_BLD C_BLUE "       at " C_PURPLE "%s:%u:%zu:\n" C_RST,
        EITHER(error->err, strerror(error->_errno)),
        error->loc.src->origin, error->loc.line, column
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
    free(line_str);
}

static void emit_errors(struct shard_context* ctx) {
    struct shard_error* errors = shard_get_errors(ctx);
    for(size_t i = 0; i < shard_get_num_errors(ctx); i++)
        print_error(&errors[i]);
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
    struct shard_context ctx = {
        .malloc = malloc,
        .realloc = realloc,
        .free = free,
        .realpath = realpath,
        .dirname = dirname,
        .access = access,
        .R_ok = R_OK,
        .W_ok = W_OK,
        .X_ok = X_OK,
        .open = _open,
        .home_dir = getenv("HOME"),
    };
    
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

    if(optind >= argc) {
        fprintf(stderr, "%s: no input files provided.\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    shard_set_current_system(&ctx, current_system);

    char* path = getenv("SHARD_PATH");
    if(path)
        shard_include_dir(&ctx, path);

    int ret = 0;
    for(; optind < argc; optind++) {
        const char* input_file = argv[optind];
        if(strcmp(input_file, "repl") == 0)
            ret = shard_repl(&ctx, echo_result);
        else
            ret = eval_file(&ctx, argv[0], input_file, echo_result);

        if(ret)
            break;
    }

    shard_deinit(&ctx);
    if(ret)
        fputs("execution terminated.\n", stderr);

    return ret;
}

