#ifndef _GEODE_H
#define _GEODE_H

#include "lifetime.h"
#include "exception.h"
#include "store.h"

#include <libshard.h>
#include <stdbool.h>

#ifndef __len
    #define __len(arr) (sizeof((arr)) / sizeof(*(arr)))
#endif

shard_dynarr(tmpfile_list, char*);

struct option_string {
    char *string;
    bool overwritten;
};

struct geode_context {
    const char *progname;

    struct option_string prefix_path;
    struct option_string store_path;
    struct option_string pkgs_path;
    struct option_string config_path;
    struct option_string module_path;

    struct shard_context shard;
    struct shard_set *prelude;

    struct geode_store store;
    struct geode_store intrinsic_store;

    lifetime_t l_global;
    struct geode_exception_handler *e_handler;

    long jobcnt;

    struct {
        bool verbose : 1;
        bool out_no_color : 1;
        bool err_no_color : 1;
        bool default_yes : 1;
        bool git_initialized : 1;
    } flags;

    struct {
        unsigned capacity, count;
        int *dfds;
    } dirstack;

    struct tmpfile_list tmpfiles;
    
    const char *initial_workdir;
};

int geode_mkcontext(struct geode_context *context, const char *progname);
void geode_delcontext(struct geode_context *context);

void geode_prelude(struct geode_context *context);
void geode_call_with_prelude(struct geode_context *context, struct shard_value *result, struct shard_value function);

void geode_set_prefix(struct geode_context *context, char *prefix_path);
void geode_set_store(struct geode_context *context, char *store_path);
void geode_set_config(struct geode_context *context, char *config_path);
void geode_set_module_dir(struct geode_context *context, char *module_path);
void geode_set_pkgs_dir(struct geode_context *context, char *pkgs_path);
int geode_set_jobcnt(struct geode_context *context, char *jobcnt);
void geode_set_verbose(struct geode_context *context, bool verbose);

void geode_load_builtins(struct geode_context *context);
void geode_apply_flags(struct geode_context *context);

// subcommands

int geode_bootstrap(struct geode_context *context, int argc, char *argv[]);
int geode_query(struct geode_context *context, int argc, char *argv[]);
int geode_list(struct geode_context *context, int argc, char *argv[]);

#endif /* _GEODE_H */

