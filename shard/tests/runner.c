#if __STDC_VERSION__ >= 199901L
#define _XOPEN_SOURCE 600
#else
#define _XOPEN_SOURCE 500
#endif /* __STDC_VERSION__ */

#include <setjmp.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

extern const char* strsignal(int);

#include <getopt.h>
#include <unistd.h>
#include <libgen.h>
#include <dirent.h>
#include <signal.h>

#include <libshard.h>

#define C_RED "\033[31m"
#define C_GREEN "\033[32m"
#define C_BLACK "\033[90m"
#define C_BLUE "\033[34m"
#define C_PURPLE "\033[35m"

#define C_BLD "\033[1m"
#define C_RST "\033[0m"
#define C_NOBLD "\033[22m"

enum test_status {
    PASSED,
    SKIPPED,
    FAILED
};

enum test_flags {
    SILENT = 1
};

static bool segv_recovery_set = false;
static jmp_buf segv_recovery;

static const struct option cmdline_options[] = {
    {"help",   0, NULL, 'h'},
    {"silent", 0, NULL, 's'},
    {"dir",    0, NULL, 'd'},
    {NULL,     0, NULL, 0  }
};

_Noreturn static void help(const char* argv0) {
    printf("Usage: %s <input file> [OPTIONS]\n\n", argv0);
    printf("Options:\n");
    printf("  -h, --help    Print this help text and exit.\n");
    printf("  -s, --silent  Supress messages except errors.\n");
    printf("  -d, --dir     Specify test directory.\n");
    exit(EXIT_SUCCESS);
}

static void _signal_handler(int signo) {
    fprintf(stderr, "\r " C_BLD C_RED "error: " C_NOBLD "uncaught signal %s (%d).\n", strsignal(signo), signo);
    if(segv_recovery_set)
        longjmp(segv_recovery, signo);

    exit(127);
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
        .tell = _tell,
        .close = _close,
        .line = 1
    };

    return 0;
}

double select_unit(double us, const char** unit) {
    if(us >= 1'000'000.0) {
        *unit = "s";
        return us / 1'000'000.0;
    }
    if(us >= 1'000.0) {
        *unit = "ms";
        return us / 1000.0;
    }
    
    *unit = "µs";
    return us;
}

void print_error(const struct shard_error* err) {
    printf("\r " C_RED C_BLD "error:" C_NOBLD " %s:%u: %s\n", err->loc.src->origin, err->loc.line + 1, err->err);
}

enum test_status run(const char* filename, struct shard_context* ctx, enum test_flags flags) {
    if(!(flags & SILENT)) {
        printf(" ⏳ %s...", filename);
        fflush(stdout);
    }

    clock_t start = clock();

    enum test_status status;

    segv_recovery_set = true;
    int sig = setjmp(segv_recovery);
    if(!sig) {
        struct shard_open_source* source = shard_open(ctx, filename);
        if(!source) {
            printf("\r " C_RED C_BLD "error:" C_NOBLD " could not read file `%s`: %s\n", filename, strerror(errno));
            status = FAILED;
            goto finish;
        }

        int num_errors = shard_eval(ctx, source);
        status = num_errors ? FAILED : PASSED;
        struct shard_error* errors = shard_get_errors(ctx);
        for(int i = 0; i < num_errors; i++)
            print_error(&errors[i]);
    }
    else {
        status = FAILED;
    }

finish:
    segv_recovery_set = false;

    shard_remove_errors(ctx);
 
    clock_t end = clock();

    if(!(flags & SILENT)) {
        switch(status) {
            case PASSED:
                printf("\r " C_GREEN C_BLD "✔ " C_RST);
                break;
            case FAILED:
                printf("\r " C_RED C_BLD "⨯ " C_RST);
                break;
            case SKIPPED:
                printf("\r " C_PURPLE C_BLD "┅ " C_RST);
                break;
        }

        const char* unit;
        double duration = select_unit((double)(end - start), &unit);
        printf("%s " C_BLACK "[%.1f %s]\n" C_RST, filename, duration, unit);
    }

    return status;
}

int run_all(const char* argv0, struct shard_context* ctx, enum test_flags flags) {
    int passed = 0, skipped = 0, failed = 0, total = 0;

    clock_t start = clock();
    
    DIR* working = opendir(".");
    if(!working) {
        fprintf(stderr, "%s: error reading working directory\n", argv0);
        return 2;
    }

    for(struct dirent* ent; (ent = readdir(working));) { 
        char* point;
        if((point = strrchr(ent->d_name, '.')) == NULL || strcmp(point, ".shard"))
            continue;

        total++;

        switch(run(ent->d_name, ctx, flags)) {
            case PASSED:
                passed++;
                break;
            case SKIPPED:
                skipped++;
                break;
            case FAILED:
                failed++;
                break;
        }
    }
    
    closedir(working);

    clock_t end = clock();

    if(!(flags & SILENT)) {
        // print summary
        printf("\n %s%-4d pass\n", passed ? C_GREEN : C_BLACK, passed);
        printf(" %s%-4d fail\n", failed ? C_RED C_BLD : C_BLACK, failed);
        printf(C_RST " %-4d skip\n", skipped);
        
        const char* unit = NULL;
        double duration = select_unit((double)(end - start), &unit);
        printf("\nRan %d tests" C_BLACK " [%.1f %s]\n\n" C_RST, total, duration, unit);
    } 

    return !!failed;
}

int main(int argc, char** argv) {
    enum test_flags flags = 0;
    int ch, long_index, err;

    while((ch = getopt_long(argc, argv, "hsd:", cmdline_options, &long_index)) != EOF) {
        switch(ch) {
        case 0:
            if(strcmp(cmdline_options[long_index].name, "help") == 0)
                goto opt_help;
            else if(strcmp(cmdline_options[long_index].name, "silent") == 0)
                goto opt_silent;
            else if(strcmp(cmdline_options[long_index].name, "dir") == 0)
                goto opt_dir;
            goto opt_unrecognized;
        case 'h':
        opt_help:
            help(argv[0]);
            break;
        case 's':
        opt_silent:
            flags |= SILENT;
            break;
        case 'd':
        opt_dir:
            if((err = chdir(optarg))) {
                fprintf(stderr, "%s: could not switch working directory to `%s`: %s\n", argv[0], optarg, strerror(err));
                exit(EXIT_FAILURE);
            }
            break;
        case '?':
        opt_unrecognized:
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

    if(signal(SIGSEGV, _signal_handler) == SIG_ERR) {
        fprintf(stderr, "%s: error setting sigsegv handler: %s\n", argv[0], strerror(errno));
        return 1;
    }

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

    if((err = shard_init(&ctx))) {
        fprintf(stderr, "%s: error initializing libshard: %s\n", argv[0], strerror(err));
        exit(EXIT_FAILURE);
    }

    int res = run_all(argv[0], &ctx, flags);
    
    shard_deinit(&ctx);
    
    return res;
}

