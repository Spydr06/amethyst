#include "log.h"
#include "exception.h"
#include "geode.h"
#include "../shard_libc_driver.h"

#include <getopt.h>
#include <stdarg.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

typedef int (*subcommand_handler_t)(struct geode_context* ctx, int argc, char **argv);

struct subcommand {
    const char *name;
    const char *desc;
    subcommand_handler_t handler;
};

static const struct subcommand cmdline_subcommands[] = {
    {"bootstrap", "Bootstrap a new root file system from a configuration file", geode_bootstrap},
    {"query",     "Search for a specific package", geode_query},
    {NULL, NULL, NULL}
};

static const struct option cmdline_options[] = {
    {"help",        no_argument,        NULL,   'h'},
    {"prefix",      required_argument,  NULL,   'p'},
    {"pkgs",        required_argument,  NULL,   'g'},
    {"store",       required_argument,  NULL,   's'},
    {"modules",     required_argument,  NULL,   'm'},
    {"verbose",     no_argument,        NULL,   'v'},
    {"jobs",        required_argument,  NULL,   'j'},
    {NULL,          0,                  NULL,   0  },
};

static void usage(const char *progname, FILE *outf) {
    fprintf(outf, "Usage: %s [subcommand] [<options...>]\n"
                  "       %s [-h | --help]\n", progname, progname);
}

static void help(struct geode_context *context) {
    usage(context->progname, stdout);
    
    printf("\nSubcommands:\n");
    
    const struct subcommand *subcmd = cmdline_subcommands;
    while(subcmd->name) {
        printf("  %s\t\t%s\n", subcmd->name, subcmd->desc);
        subcmd++;
    }

    printf("\nOptions:\n");
    printf("  -h, --help              Print this help text and exit.\n");
    printf("  -p, --prefix <path>     Set the system prefix. [%s]\n", context->prefix_path.string);
    printf("  -g, --pkgs <path>       Set the system package path. [%s]", context->pkgs_path.string);
    printf("  -s, --store <path>      Set the system store path. [%s]\n", context->store_path.string);
    printf("  -v, --verbose           Enable verbose output.\n");
    printf("  -y                      Answer `yes' by default.\n");

    printf("\n");
}

static void default_exception_handler(struct geode_context *context, volatile exception_t *e) {
    switch(e->type) {
    case GEODE_EX_SUBCOMMAND:
        geode_errorf(context, "%s", e->description);
	usage(context->progname, stderr);
        break;
    case GEODE_EX_SHARD:
        geode_errorf(context, "%s", e->description); 
        print_shard_error(stderr, e->payload.shard.errors);
        break;
    case GEODE_EX_IO:
        geode_errorf(context, "%s: %s", e->description, strerror(e->payload.ioerrno));
        break;
    case GEODE_EX_GIT:
        geode_errorf(context, "%s: (%d) %s", e->description, e->payload.git.error->klass, e->payload.git.error->message);
        break;
    default:
        if(e->description)
            geode_panic(context, "Unknown exception `%s': %s", exception_type_to_string(e->type), e->description);
        else
            geode_panic(context, "Unknwon exception `%s'", exception_type_to_string(e->type));
    }

    if(e->stacktrace_size)
        geode_print_stacktrace(context, (void**) e->stacktrace, e->stacktrace_size);
}

int main(int argc, char *argv[]) {
    volatile int err, ret = EXIT_SUCCESS;
    struct geode_context context;
    geode_mkcontext(&context, argv[0]);

    volatile exception_t *e = geode_catch(&context, GEODE_ANY_EXCEPTION);
    if(e) {
        default_exception_handler(&context, e);
        ret = EXIT_FAILURE;
        goto cleanup;
    }

    geode_load_builtins(&context);

    int c; 
    while((c = getopt_long(argc, argv, "vp:c:g:s:m:j:h", cmdline_options, NULL)) != EOF) {
        switch(c) {
        case 'p':
            geode_set_prefix(&context, optarg);
            break;
        case 's':
            geode_set_store(&context, optarg);
            break;
        case 'm':
            geode_set_module_dir(&context, optarg);
            break;
        case 'j':
            if((err = geode_set_jobcnt(&context, optarg))) {
                fprintf(stderr, "%s: invalid format of argument passed to option `-j'/`--jobs': %s", context.progname, strerror(err));
                ret = EXIT_FAILURE;
                goto cleanup;
            }
            break;
        case 'h':
            help(&context);
            goto cleanup;
        case 'v':
            geode_set_verbose(&context, true);
            break;
        case 'g':
            geode_set_pkgs_dir(&context, optarg);
            break;
        case 'y':
            context.flags.default_yes = true;
            break;
        case '?':
        default:
            fprintf(stderr, "Try '%s --help' for more information.\n", context.progname);
        }   
    }

    if(optind >= argc)
        geode_throwf(&context, GEODE_EX_SUBCOMMAND, "No subcommand specified.");

    const struct subcommand *subcommand = cmdline_subcommands;

    while(subcommand->name) {
        if(strcmp(subcommand->name, argv[optind]) == 0)
            break;
        subcommand++;
    }
    
    if(!subcommand->name)
        geode_throwf(&context, GEODE_EX_SUBCOMMAND, "Unknown subcommand `%s'.", argv[optind]);

    geode_prelude(&context);

    ret = subcommand->handler(&context, argc - (optind + 1), argv + optind + 1);

cleanup:
    geode_pop_exception_handler(&context);
    geode_delcontext(&context);
    return ret;
}

