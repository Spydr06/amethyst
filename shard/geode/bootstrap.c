#include <bootstrap.h>
#include <geode.h>
#include <config.h>
#include <context.h>
#include <package.h>

#include <assert.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

typedef void (*argument_handler_t)(struct geode_context* ctx, struct bootstrap_flags* flags, const char* value);

struct bootstrap_argument {
    const char* name;
    const char* desc;
    bool expect_arg;
    argument_handler_t handler;
};

static void help(struct geode_context* ctx, struct bootstrap_flags* flags, const char*);
static void enable_initrd(struct geode_context* ctx, struct bootstrap_flags* flags, const char* path);

static const struct bootstrap_argument cmdline_arguments[] = {
    {"help",   "         Display `bootstrap` help text.", false, help},
    {"initrd", "<path> Generate an initrd file.", true, enable_initrd},
    {NULL, NULL, false, NULL}
};

static void help(struct geode_context* ctx, struct bootstrap_flags*, const char*) {
    printf("Usage: %s bootstrap [<ARGUMENTS>]\n\n", ctx->prog_name);
    printf("Arguments:\n");

    const struct bootstrap_argument* arg = cmdline_arguments;
    while(arg->name) {
        printf("  %s %s\n", arg->name, arg->desc); 
        arg++;
    }

    printf("\nFor general help, try `%s --help`.\n", ctx->prog_name);

    exit(EXIT_SUCCESS);
}

static void enable_initrd(struct geode_context*, struct bootstrap_flags* flags, const char* path) {
    assert(path);
    flags->initrd_path = path;
}

static void create_prefix(struct geode_context* ctx) {
    int err = mkdir(ctx->prefix, 0700);
    if(err < 0 && errno != EEXIST)
        geode_throw(ctx, MKDIR, .file=TUPLE(ctx->prefix, errno));
}

static char* get_bootstrap_script_path(struct geode_context* ctx) {
    size_t path_len = strlen(ctx->store_path) + strlen(GEODE_BOOSTRAP_FILE) + 2;
    char* path = geode_malloc(ctx, path_len);
    snprintf(path, path_len, "%s/%s", ctx->store_path, GEODE_BOOSTRAP_FILE);

    return path;
}

int geode_bootstrap(struct geode_context* ctx, int argc, char** argv) {
    struct bootstrap_flags flags = {0};

    for(int i = 0; i < argc; i++) {
        char* arg = argv[i];

        const struct bootstrap_argument* cur = cmdline_arguments;
        bool found = false;
        while(cur->name) {
            if(strcmp(cur->name, arg) == 0) {
                found = true;
                break;
            }
            cur++;
        }

        if(!found)
            geode_throw(ctx, UNRECOGNIZED_ARGUMENT, .argument=arg);            

        const char* param = NULL;
        if(cur->expect_arg) {
            if(argc - i <= 1)
                geode_throw(ctx, MISSING_PARAMETER, .argument=arg);
            param = argv[++i];
        }

        cur->handler(ctx, &flags, param);
    }

    infof(ctx, "Bootstrapping `%s` to `%s`... (This could take a while)\n", ctx->main_config_path, ctx->prefix);    

    create_prefix(ctx);
    geode_load_config(ctx);

    struct package_index index;
    int err = geode_index_packages(ctx, &index);

/*    char* bootstrap_script_path = get_bootstrap_script_path(ctx);
    struct shard_value boostrap_result = geode_call_file(ctx, bootstrap_script_path);

    struct shard_string str = {0};
    int err = shard_value_to_string(&ctx->shard_ctx, &str, &boostrap_result, 10);
    shard_string_push(&ctx->shard_ctx, &str, '\0');

    printf("Bootstrap result: %s\n", str.items);

    shard_string_free(&ctx->shard_ctx, &str);

    if(err)
        geode_throw(ctx, SHARD, .shard=TUPLE(.errs=shard_get_errors(&ctx->shard_ctx), .num=err));
*/
    return 0;
}

