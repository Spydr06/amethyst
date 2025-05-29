#include "geode.h"
#include "log.h"
#include "derivation.h"
#include "git.h"
#include "net.h"
#include "archives.h"

#include <libshard.h>

#include <assert.h>
#include <dirent.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <libgen.h>
#include <stdio.h>

#include <sys/stat.h>
#include <sys/wait.h>
#include <sys/types.h>

static struct shard_value builtin_debug_dump(volatile struct shard_evaluator* e, struct shard_builtin* builtin, struct shard_lazy_value** args);
static struct shard_value builtin_debug_println(volatile struct shard_evaluator* e, struct shard_builtin* builtin, struct shard_lazy_value** args);
static struct shard_value builtin_debug_unimplemented(volatile struct shard_evaluator* e, struct shard_builtin* builtin, struct shard_lazy_value** args);
static struct shard_value builtin_file_basename(volatile struct shard_evaluator* e, struct shard_builtin* builtin, struct shard_lazy_value** args);
static struct shard_value builtin_file_dirname(volatile struct shard_evaluator* e, struct shard_builtin* builtin, struct shard_lazy_value** args);
static struct shard_value builtin_file_exists(volatile struct shard_evaluator* e, struct shard_builtin* builtin, struct shard_lazy_value** args);
static struct shard_value builtin_file_mkdir(volatile struct shard_evaluator* e, struct shard_builtin* builtin, struct shard_lazy_value** args);
static struct shard_value builtin_file_writeFile(volatile struct shard_evaluator* e, struct shard_builtin* builtin, struct shard_lazy_value** args);
static struct shard_value builtin_file_readDir(volatile struct shard_evaluator* e, struct shard_builtin* builtin, struct shard_lazy_value** args);
static struct shard_value builtin_errno_toString(volatile struct shard_evaluator* e, struct shard_builtin* builtin, struct shard_lazy_value** args);
static struct shard_value builtin_error_throw(volatile struct shard_evaluator* e, struct shard_builtin* builtin, struct shard_lazy_value** args);
static struct shard_value builtin_proc_spawn(volatile struct shard_evaluator* e, struct shard_builtin* builtin, struct shard_lazy_value** args);
static struct shard_value builtin_proc_spawnPipe(volatile struct shard_evaluator* e, struct shard_builtin* builtin, struct shard_lazy_value** args);
static struct shard_value builtin_setenv(volatile struct shard_evaluator* e, struct shard_builtin* builtin, struct shard_lazy_value** args);
static struct shard_value builtin_getenv(volatile struct shard_evaluator* e, struct shard_builtin* builtin, struct shard_lazy_value** args);
static struct shard_value builtin_unsetenv(volatile struct shard_evaluator* e, struct shard_builtin* builtin, struct shard_lazy_value** args);

static struct shard_builtin geode_builtin_functions[] = {
    SHARD_BUILTIN("geode.archive.extractTar", builtin_archive_extractTar, SHARD_VAL_PATH, SHARD_VAL_PATH),
    SHARD_BUILTIN("geode.debug.dump", builtin_debug_dump, SHARD_VAL_ANY),
    SHARD_BUILTIN("geode.debug.println", builtin_debug_println, SHARD_VAL_STRING),
    SHARD_BUILTIN("geode.debug.unimplemented", builtin_debug_unimplemented, SHARD_VAL_STRING),
    SHARD_BUILTIN("geode.file.basename", builtin_file_basename, SHARD_VAL_PATH),
    SHARD_BUILTIN("geode.file.dirname", builtin_file_dirname, SHARD_VAL_PATH),
    SHARD_BUILTIN("geode.file.exists", builtin_file_exists, SHARD_VAL_PATH),
    SHARD_BUILTIN("geode.file.mkdir", builtin_file_mkdir, SHARD_VAL_PATH),
    SHARD_BUILTIN("geode.file.writeFile", builtin_file_writeFile, SHARD_VAL_PATH, SHARD_VAL_STRING),
    SHARD_BUILTIN("geode.file.readDir", builtin_file_readDir, SHARD_VAL_PATH),
    SHARD_BUILTIN("geode.errno.toString", builtin_errno_toString, SHARD_VAL_INT),
    SHARD_BUILTIN("geode.error.throw", builtin_error_throw, SHARD_VAL_STRING),
    SHARD_BUILTIN("geode.git.checkoutBranch", builtin_git_checkoutBranch, SHARD_VAL_STRING, SHARD_VAL_PATH),
    SHARD_BUILTIN("geode.git.pullRepo", builtin_git_pullRepo, SHARD_VAL_PATH),
    SHARD_BUILTIN("geode.git.cloneRepo", builtin_git_cloneRepo, SHARD_VAL_STRING, SHARD_VAL_PATH),
    SHARD_BUILTIN("geode.net.downloadTmp", builtin_net_downloadTmp, SHARD_VAL_STRING),
    SHARD_BUILTIN("geode.proc.spawn", builtin_proc_spawn, SHARD_VAL_STRING, SHARD_VAL_LIST, SHARD_VAL_BOOL),
    SHARD_BUILTIN("geode.proc.spawnPipe", builtin_proc_spawnPipe, SHARD_VAL_STRING, SHARD_VAL_LIST, SHARD_VAL_STRING),
    SHARD_BUILTIN("geode.setenv", builtin_setenv, SHARD_VAL_STRING, SHARD_VAL_STRING),
    SHARD_BUILTIN("geode.getenv", builtin_getenv, SHARD_VAL_STRING),
    SHARD_BUILTIN("geode.unsetenv", builtin_unsetenv, SHARD_VAL_STRING),
    SHARD_BUILTIN("geode.derivation", geode_builtin_derivation, SHARD_VAL_SET),
    SHARD_BUILTIN("geode.storeEntry", geode_builtin_storeEntry, SHARD_VAL_PATH),
    SHARD_BUILTIN("geode.intrinsicStore", geode_builtin_intrinsicStore, SHARD_VAL_SET),
};

static void load_constants(struct geode_context* ctx) {
    struct {
        const char* ident;
        struct shard_value value;
    } builtin_constants[] = {
        {"geode.prefix", (struct shard_value){.type=SHARD_VAL_PATH, .path=ctx->prefix_path.string, .pathlen=strlen(ctx->prefix_path.string)}},
        {"geode.storeDir", (struct shard_value){.type=SHARD_VAL_PATH, .path=ctx->store_path.string, .pathlen=strlen(ctx->store_path.string)}},
        {"geode.pkgsDir", (struct shard_value){.type=SHARD_VAL_PATH, .path=ctx->pkgs_path.string, .pathlen=strlen(ctx->pkgs_path.string)}},
        {"geode.moduleDir", (struct shard_value){.type=SHARD_VAL_PATH, .path=ctx->module_path.string, .pathlen=strlen(ctx->module_path.string)}},
        {"geode.architecture", (struct shard_value){.type=SHARD_VAL_STRING, .string="x86_64", .strlen=6}}, // TODO: make properly
        {"geode.proc.nJobs", (struct shard_value){.type=SHARD_VAL_INT, .integer=ctx->jobcnt}},
    };

    for(size_t i = 0; i < __len(builtin_constants); i++) {
        int err = shard_define_constant(
            &ctx->shard,
            builtin_constants[i].ident,
            builtin_constants[i].value
        );

        assert(err == 0 && "shard_define_constant returned error");
    }
}

void geode_load_builtins(struct geode_context* ctx) {
    load_constants(ctx);

    for(size_t i = 0; i < __len(geode_builtin_functions); i++) {
        int err = shard_define_builtin(
            &ctx->shard, 
            &geode_builtin_functions[i]
        );

        assert(err == 0 && "shard_define_function returned error");
    }
}

static struct shard_value builtin_debug_dump(volatile struct shard_evaluator* e, struct shard_builtin* builtin, struct shard_lazy_value** args) {
    struct shard_value arg = shard_builtin_eval_arg(e, builtin, args, 0);
    struct geode_context* geode_ctx = e->ctx->userp;

    struct shard_string str = {0};
    int err = shard_value_to_string(e->ctx, &str, &arg, 10);
    if(err != 0)
        geode_throw(geode_ctx, geode_shard_ex(e->ctx->userp));
    shard_string_push(e->ctx, &str, '\0');

    geode_infof(geode_ctx, "`geode.debug.dump`: %s\n", str.items);

    shard_string_free(e->ctx, &str);

    return arg;
}

static struct shard_value builtin_debug_println(volatile struct shard_evaluator* e, struct shard_builtin* builtin, struct shard_lazy_value** args) {
    struct shard_value arg = shard_builtin_eval_arg(e, builtin, args, 0);

    fputs(arg.string, stdout);
    fputc('\n', stdout);

    return arg;
}

static struct shard_value builtin_debug_unimplemented(volatile struct shard_evaluator* e, struct shard_builtin* builtin, struct shard_lazy_value** args) {
    struct shard_value msg = shard_builtin_eval_arg(e, builtin, args, 0);

    shard_eval_throw(e, e->error_scope->loc, "`geode.debug.unimplemented` called unimplemented value: %s", msg.string);
}

static struct shard_value builtin_file_basename(volatile struct shard_evaluator* e, struct shard_builtin* builtin, struct shard_lazy_value** args) {
    struct shard_value arg = shard_builtin_eval_arg(e, builtin, args, 0);

    char* path_copy = strdup(arg.path);
    char* base = basename(path_copy);
    size_t base_len = strlen(base);

    char* gc_base = shard_gc_malloc(e->gc, base_len + 1);
    memcpy(gc_base, base, base_len);

    free(path_copy);

    return (struct shard_value){.type=SHARD_VAL_PATH, .path=gc_base, .pathlen=base_len};
}

static struct shard_value builtin_file_dirname(volatile struct shard_evaluator* e, struct shard_builtin* builtin, struct shard_lazy_value** args) {
    struct shard_value arg = shard_builtin_eval_arg(e, builtin, args, 0);

    char* path_copy = strdup(arg.path);
    char* dir = dirname(path_copy);
    size_t dir_len = strlen(dir);

    char* gc_dir = shard_gc_malloc(e->gc, dir_len + 1);
    memcpy(gc_dir, dir, dir_len);

    free(path_copy);

    return (struct shard_value){.type=SHARD_VAL_PATH, .path=gc_dir, .pathlen=dir_len};
}

static struct shard_value builtin_file_exists(volatile struct shard_evaluator* e, struct shard_builtin* builtin, struct shard_lazy_value** args) {
    struct shard_value arg = shard_builtin_eval_arg(e, builtin, args, 0);
    
    return (struct shard_value){.type=SHARD_VAL_BOOL, .boolean=access(arg.path, F_OK) == 0};
}

static struct shard_value builtin_file_mkdir(volatile struct shard_evaluator* e, struct shard_builtin* builtin, struct shard_lazy_value** args) {
    struct shard_value arg = shard_builtin_eval_arg(e, builtin, args, 0);

    return (struct shard_value){.type=SHARD_VAL_INT, .integer=mkdir(arg.path, 0777)};
}

static struct shard_value builtin_file_writeFile(volatile struct shard_evaluator* e, struct shard_builtin* builtin, struct shard_lazy_value** args) {
    struct shard_value file = shard_builtin_eval_arg(e, builtin, args, 0);
    struct shard_value data = shard_builtin_eval_arg(e, builtin, args, 1);

    FILE* fp = fopen(file.string, "w");
    if(!fp)
        return (struct shard_value){.type=SHARD_VAL_INT, .integer=errno};

    fwrite(data.string, data.strlen, sizeof(char), fp);
    int err = fclose(fp);

    return (struct shard_value){.type=SHARD_VAL_INT, .integer=err};
}

static struct shard_value builtin_file_readDir(volatile struct shard_evaluator* e, struct shard_builtin* builtin, struct shard_lazy_value** args) {
    struct shard_value path = shard_builtin_eval_arg(e, builtin, args, 0);

    DIR* dir = opendir(path.path);
    if(!dir)
        return (struct shard_value){.type=SHARD_VAL_INT, .integer=errno};

    struct dirent *entry;
    size_t dirsize = 0;

    while((entry = readdir(dir)))
        dirsize++; 

    rewinddir(dir);
    struct shard_set *entries = shard_set_init(e->ctx, dirsize);

    while((entry = readdir(dir))) {
        shard_ident_t name = shard_get_ident(e->ctx, entry->d_name);

        const char *value = "unknown";
        switch(entry->d_type) {
        case DT_FIFO:
            value = "fifo";
            break;
        case DT_CHR:
            value = "chardev";
            break;
        case DT_DIR:
            value = "directory";
            break;
        case DT_BLK:
            value = "blockdev";
            break;
        case DT_REG:
            value = "regular";
            break;
        case DT_LNK:
            value = "link";
            break;
        case DT_SOCK:
            value = "socket";
            break;
        default:
            value = "unknown";
            break;
        }

        shard_set_put(entries, name, shard_unlazy(e->ctx, (struct shard_value){.type=SHARD_VAL_STRING, .string=value, .strlen=strlen(value)}));
    }

    closedir(dir);
    return (struct shard_value){.type=SHARD_VAL_SET, .set=entries};
}

static struct shard_value builtin_errno_toString(volatile struct shard_evaluator* e, struct shard_builtin* builtin, struct shard_lazy_value** args) {
    struct shard_value arg = shard_builtin_eval_arg(e, builtin, args, 0);

    const char* strerr = strerror(arg.integer);
    assert(strerr != NULL && "strerror() should never return NULL");

    return (struct shard_value){.type=SHARD_VAL_STRING, .string=strerr, .strlen=strlen(strerr)};
}

static struct shard_value builtin_error_throw(volatile struct shard_evaluator* e, struct shard_builtin* builtin, struct shard_lazy_value** args) {
    struct shard_value msg = shard_builtin_eval_arg(e, builtin, args, 0);

    struct geode_context* ctx = e->ctx->userp;

    struct shard_error* err = l_malloc(&ctx->l_global, sizeof(struct shard_error));
    *err = (struct shard_error){
        .err = l_strdup(&ctx->l_global, msg.string),
        .loc = e->error_scope->loc,
        .heap = false,
        ._errno = 0
    };

    geode_throw(ctx, geode_shard_ex2(ctx, 1, err));
}

static struct shard_value builtin_proc_spawn(volatile struct shard_evaluator* e, struct shard_builtin* builtin, struct shard_lazy_value** args) {
    struct shard_value exec = shard_builtin_eval_arg(e, builtin, args, 0);
    struct shard_value arg_list = shard_builtin_eval_arg(e, builtin, args, 1);
    struct shard_value do_wait = shard_builtin_eval_arg(e, builtin, args, 2);

    size_t argc = shard_list_length(arg_list.list.head) + 1;
    char** argv = malloc(sizeof(char*) * (argc + 1));

    argv[0] = (char*) exec.string;

    size_t counter = 1;
    for(struct shard_list* arg = arg_list.list.head; arg; arg = arg->next) {
        struct shard_value value = shard_eval_lazy2(e, arg->value);
        if(value.type != SHARD_VAL_STRING)
            shard_eval_throw(e, 
                    e->error_scope->loc,
                    "`geode.proc.spawn` expects argument `2` to be a list of strings, got element `%zu` of type `%s`.",
                    counter - 1,
                    shard_value_type_to_string(e->ctx, value.type)
                );

        argv[counter++] = (char*) value.string;
    }

    assert(counter == argc);
    argv[argc] = NULL;

    struct geode_context* ctx = e->ctx->userp;
    if(ctx->flags.verbose) {
        geode_verbosef(ctx, "proc.spawn: ");
        for(char** arg = argv; *arg; arg++) {
            printf("%s ", *arg);
        }
        printf("\n");
    }

    struct shard_value return_val = {.type=SHARD_VAL_INT, .integer = 255};

    pid_t pid = fork();
    if(pid == 0) {
        execvp(exec.string, argv);
        exit(255);
    }
    else if(pid < 0)
        errno = -pid;
    else if(do_wait.boolean) {
        int pid_status;
        waitpid(pid, &pid_status, 0);
 
        if(WIFEXITED(pid_status))
            return_val.integer = WEXITSTATUS(pid_status);
        else if(WIFSIGNALED(pid_status))
            geode_verbosef(ctx, "proc.spawn: '%s' terminated with signal %s (%d)\n", exec.string, strsignal(WTERMSIG(pid_status)), WTERMSIG(pid_status));
        else if(WIFSTOPPED(pid_status))
            geode_verbosef(ctx, "proc.spawn: '%s' terminated with signal %s (%d)\n", exec.string, strsignal(WSTOPSIG(pid_status)), WSTOPSIG(pid_status));
    }
    else
        return_val.integer = pid;

    free(argv);

    return return_val;
}

static struct shard_value builtin_proc_spawnPipe(volatile struct shard_evaluator* e, struct shard_builtin* builtin, struct shard_lazy_value** args) {
    struct shard_value exec = shard_builtin_eval_arg(e, builtin, args, 0);
    struct shard_value arg_list = shard_builtin_eval_arg(e, builtin, args, 1);
    struct shard_value input_string = shard_builtin_eval_arg(e, builtin, args, 2);

    size_t argc = shard_list_length(arg_list.list.head) + 1;
    char** argv = malloc(sizeof(char*) * (argc + 1));

    argv[0] = (char*) exec.string;

    size_t counter = 1;
    for(struct shard_list* arg = arg_list.list.head; arg; arg = arg->next) {
        struct shard_value value = shard_eval_lazy2(e, arg->value);
        if(value.type != SHARD_VAL_STRING)
            shard_eval_throw(e, 
                    e->error_scope->loc,
                    "`geode.proc.spawnPipe` expects argument `2` to be a list of strings, got element `%zu` of type `%s`.",
                    counter - 1,
                    shard_value_type_to_string(e->ctx, value.type)
                );

        argv[counter++] = (char*) value.string;
    }

    assert(counter == argc);
    argv[argc] = NULL;

    struct geode_context* ctx = e->ctx->userp;
    if(ctx->flags.verbose) {
        geode_verbosef(ctx, "proc.spawnPipe: ");
        for(char** arg = argv; *arg; arg++) {
            printf("%s ", *arg);
        }
        printf("\n");
    }

    int stdin_pipe[2], stdout_pipe[2], stderr_pipe[2];
    int exit_code = 255;
    char buffer[256];
    struct shard_string out = {0}, err = {0};

    assert(pipe(stdin_pipe) >= 0);
    assert(pipe(stdout_pipe) >= 0);
    assert(pipe(stderr_pipe) >= 0);

    pid_t pid = fork();
    if(pid == 0) {
        close(stdout_pipe[0]);
        close(stderr_pipe[0]);
        close(stdin_pipe[1]);

        dup2(stdout_pipe[1], STDOUT_FILENO);
        dup2(stderr_pipe[1], STDERR_FILENO);
        dup2(stdin_pipe[1], STDIN_FILENO);

        close(stdout_pipe[1]);
        close(stderr_pipe[1]);

        execvp(exec.string, argv);
        exit(255);
    }
    else if(pid < 0)
        errno = -pid;
    else {
        close(stdout_pipe[1]);
        close(stderr_pipe[1]);
        close(stdin_pipe[0]);

        assert(write(stdin_pipe[1], input_string.string, input_string.strlen) == (ssize_t) input_string.strlen);
        close(stdin_pipe[1]);

        int bytes_read;
        while((bytes_read = read(stdout_pipe[0], buffer, sizeof(buffer))) > 0)
            shard_gc_string_appendn(e->gc, &out, buffer, bytes_read);
        shard_gc_string_push(e->gc, &out, '\0');

        while((bytes_read = read(stderr_pipe[0], buffer, sizeof(buffer))) > 0)
            shard_gc_string_appendn(e->gc, &err, buffer, bytes_read);
        shard_gc_string_push(e->gc, &err, '\0');

        close(stdout_pipe[0]);
        close(stderr_pipe[0]);

        int pid_status;
        waitpid(pid, &pid_status, 0);

        if(WIFEXITED(pid_status))
            exit_code = WEXITSTATUS(pid_status);
        else if(WIFSIGNALED(pid_status))
            geode_verbosef(ctx, "proc.spawn: '%s' terminated with signal %s (%d)\n", exec.string, strsignal(WTERMSIG(pid_status)), WTERMSIG(pid_status));
        else if(WIFSTOPPED(pid_status))
            geode_verbosef(ctx, "proc.spawn: '%s' terminated with signal %s (%d)\n", exec.string, strsignal(WSTOPSIG(pid_status)), WSTOPSIG(pid_status));
    } 

    struct shard_set* result = shard_set_init(e->ctx, 3);

    shard_set_put(result, shard_get_ident(e->ctx, "exitCode"), shard_unlazy(e->ctx, (struct shard_value){.type=SHARD_VAL_INT, .integer=exit_code}));
    shard_set_put(result, shard_get_ident(e->ctx, "stdout"), shard_unlazy(e->ctx, (struct shard_value){.type=SHARD_VAL_STRING, .string=out.items, .strlen=out.count - 1}));
    shard_set_put(result, shard_get_ident(e->ctx, "stderr"), shard_unlazy(e->ctx, (struct shard_value){.type=SHARD_VAL_STRING, .string=err.items, .strlen=err.count - 1}));
    
    free(argv);

    return (struct shard_value){.type=SHARD_VAL_SET, .set=result};
}

static struct shard_value builtin_setenv(volatile struct shard_evaluator* e, struct shard_builtin* builtin, struct shard_lazy_value** args) {
    struct shard_value var = shard_builtin_eval_arg(e, builtin, args, 0);
    struct shard_value val = shard_builtin_eval_arg(e, builtin, args, 1);

    return (struct shard_value){.type=SHARD_VAL_INT, .integer=setenv(var.string, val.string, true)};
}

static struct shard_value builtin_getenv(volatile struct shard_evaluator* e, struct shard_builtin* builtin, struct shard_lazy_value** args) {
    struct shard_value arg = shard_builtin_eval_arg(e, builtin, args, 0);

    const char* val = getenv(arg.string);
    if(!val)
        return (struct shard_value){.type=SHARD_VAL_NULL};

    return (struct shard_value){.type=SHARD_VAL_STRING, .string=val, .strlen=strlen(val)};
}

static struct shard_value builtin_unsetenv(volatile struct shard_evaluator* e, struct shard_builtin* builtin, struct shard_lazy_value** args) {
    struct shard_value arg = shard_builtin_eval_arg(e, builtin, args, 0);

    return (struct shard_value){.type=SHARD_VAL_INT,.integer=unsetenv(arg.string)};
}

