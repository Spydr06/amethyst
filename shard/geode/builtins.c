#define _POSIX_C_SOURCE 200809L

#include <geode.h>
#include <context.h>
#include <config.h>

#include <libshard.h>

#include <assert.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <libgen.h>

#include <sys/stat.h>
#include <sys/wait.h>
#include <sys/types.h>

static struct shard_value builtin_debug_dump(volatile struct shard_evaluator* e, struct shard_lazy_value** args, struct shard_location* loc);
static struct shard_value builtin_debug_println(volatile struct shard_evaluator* e, struct shard_lazy_value** args, struct shard_location* loc);
static struct shard_value builtin_debug_unimplemented(volatile struct shard_evaluator* e, struct shard_lazy_value** args, struct shard_location* loc);
static struct shard_value builtin_file_basename(volatile struct shard_evaluator* e, struct shard_lazy_value** args, struct shard_location* loc);
static struct shard_value builtin_file_dirname(volatile struct shard_evaluator* e, struct shard_lazy_value** args, struct shard_location* loc);
static struct shard_value builtin_file_exists(volatile struct shard_evaluator* e, struct shard_lazy_value** args, struct shard_location* loc);
static struct shard_value builtin_file_mkdir(volatile struct shard_evaluator* e, struct shard_lazy_value** args, struct shard_location* loc);
static struct shard_value builtin_errno_toString(volatile struct shard_evaluator* e, struct shard_lazy_value** args, struct shard_location* loc);
static struct shard_value builtin_error_throw(volatile struct shard_evaluator* e, struct shard_lazy_value** args, struct shard_location* loc);
static struct shard_value builtin_proc_spawn(volatile struct shard_evaluator* e, struct shard_lazy_value** args, struct shard_location* loc);
static struct shard_value builtin_proc_spawnPipe(volatile struct shard_evaluator* e, struct shard_lazy_value** args, struct shard_location* loc);

static const struct {
    const char* ident;
    struct shard_value (*callback)(volatile struct shard_evaluator*, struct shard_lazy_value**, struct shard_location*);
    unsigned arity;
} geode_builtin_functions[] = {
    {"geode.debug.dump", builtin_debug_dump, 1},
    {"geode.debug.println", builtin_debug_println, 1},
    {"geode.debug.unimplemented", builtin_debug_unimplemented, 1},
    {"geode.file.basename", builtin_file_basename, 1},
    {"geode.file.dirname", builtin_file_dirname, 1},
    {"geode.file.exists", builtin_file_exists, 1},
    {"geode.file.mkdir", builtin_file_mkdir, 1},
    {"geode.errno.toString", builtin_errno_toString, 1},
    {"geode.error.throw", builtin_error_throw, 1},
    {"geode.proc.spawn", builtin_proc_spawn, 3},
    {"geode.proc.spawnPipe", builtin_proc_spawnPipe, 3},
};

static void load_constants(struct geode_context* ctx) {
    struct {
        const char* ident;
        struct shard_value value;
    } builtin_constants[] = {
        {"geode.prefix", (struct shard_value){.type=SHARD_VAL_PATH, .path=ctx->prefix, .pathlen=strlen(ctx->prefix)}},
        {"geode.store", (struct shard_value){.type=SHARD_VAL_PATH, .path=ctx->store_path, .pathlen=strlen(ctx->store_path)}},
        {"geode.architecture", (struct shard_value){.type=SHARD_VAL_STRING, .string="x86_64", .strlen=6}}, // TODO: make properly
        {"geode.proc.nJobs", (struct shard_value){.type=SHARD_VAL_INT, .integer=ctx->nproc}},
    };

    for(size_t i = 0; i < __len(builtin_constants); i++) {
        int err = shard_define_constant(
            &ctx->shard_ctx,
            builtin_constants[i].ident,
            builtin_constants[i].value
        );

        assert(err == 0 && "shard_define_constant returned error");
    }
}

void geode_load_builtins(struct geode_context* ctx) {
    load_constants(ctx);

    for(size_t i = 0; i < __len(geode_builtin_functions); i++) {
        int err = shard_define_function(
            &ctx->shard_ctx, 
            geode_builtin_functions[i].ident, 
            geode_builtin_functions[i].callback, 
            geode_builtin_functions[i].arity
        );

        assert(err == 0 && "shard_define_function returned error");
    }
}

static struct shard_value builtin_debug_dump(volatile struct shard_evaluator* e, struct shard_lazy_value** args, struct shard_location*) {
    struct shard_value arg = shard_eval_lazy2(e, *args);

    struct geode_context* geode_ctx = e->ctx->userp;

    struct shard_string str = {0};
    int err = shard_value_to_string(e->ctx, &str, &arg, 10);
    if(err != 0)
        geode_throw(geode_ctx, SHARD, .shard=TUPLE(.num=err, .errs=shard_get_errors(e->ctx)));
    shard_string_push(e->ctx, &str, '\0');

    infof(geode_ctx, "`geode.debug.dump`: %s\n", str.items);

    shard_string_free(e->ctx, &str);

    return arg;
}

static struct shard_value builtin_debug_println(volatile struct shard_evaluator* e, struct shard_lazy_value** args, struct shard_location* loc) {
    struct shard_value arg = shard_eval_lazy2(e, *args);
    if(arg.type != SHARD_VAL_STRING)
        shard_eval_throw(e, *loc, "`geode.debug.println` expects argument to be of type `string`");

    fputs(arg.string, stdout);
    fputc('\n', stdout);

    return arg;
}

static struct shard_value builtin_debug_unimplemented(volatile struct shard_evaluator* e, struct shard_lazy_value** args, struct shard_location* loc) {
    struct shard_value msg = shard_eval_lazy2(e, *args);
    if(msg.type != SHARD_VAL_STRING)
        shard_eval_throw(e, *loc, "`geode.debug.unimplemented` expects argument to be of type `string`");

    shard_eval_throw(e, *loc, "`geode.debug.unimplemented` called unimplemented value: %s", msg.string);
}

static struct shard_value builtin_file_basename(volatile struct shard_evaluator* e, struct shard_lazy_value** args, struct shard_location* loc) {
    struct shard_value arg = shard_eval_lazy2(e, *args);
    if(arg.type != SHARD_VAL_PATH)
        shard_eval_throw(e, *loc, "`geode.file.basename` expects argument to be of type `path`");

    char* path_copy = strdup(arg.path);
    char* base = basename(path_copy);
    size_t base_len = strlen(base);

    char* gc_base = shard_gc_malloc(e->gc, base_len + 1);
    memcpy(gc_base, base, base_len);

    free(path_copy);

    return (struct shard_value){.type=SHARD_VAL_PATH, .path=gc_base, .pathlen=base_len};
}

static struct shard_value builtin_file_dirname(volatile struct shard_evaluator* e, struct shard_lazy_value** args, struct shard_location* loc) {
    struct shard_value arg = shard_eval_lazy2(e, *args);
    if(arg.type != SHARD_VAL_PATH)
        shard_eval_throw(e, *loc, "`geode.file.basename` expects argument to be of type `path`");

    char* path_copy = strdup(arg.path);
    char* dir = dirname(path_copy);
    size_t dir_len = strlen(dir);

    char* gc_dir = shard_gc_malloc(e->gc, dir_len + 1);
    memcpy(gc_dir, dir, dir_len);

    free(path_copy);

    return (struct shard_value){.type=SHARD_VAL_PATH, .path=gc_dir, .pathlen=dir_len};
}

static struct shard_value builtin_file_exists(volatile struct shard_evaluator* e, struct shard_lazy_value** args, struct shard_location* loc) {
    struct shard_value arg = shard_eval_lazy2(e, *args);
    if(arg.type != SHARD_VAL_PATH)
        shard_eval_throw(e, *loc, "`geode.file.exists` expects argument to be of type `path`");
    
    return (struct shard_value){.type=SHARD_VAL_BOOL, .boolean=access(arg.path, F_OK) == 0};
}

static struct shard_value builtin_file_mkdir(volatile struct shard_evaluator* e, struct shard_lazy_value** args, struct shard_location* loc) {
    struct shard_value arg = shard_eval_lazy2(e, *args);
    if(arg.type != SHARD_VAL_PATH)
        shard_eval_throw(e, *loc, "`geode.file.mkdir` expects argument to be of type `path`");

    return (struct shard_value){.type=SHARD_VAL_INT, .integer=mkdir(arg.path, 0777)};
}

static struct shard_value builtin_errno_toString(volatile struct shard_evaluator* e, struct shard_lazy_value** args, struct shard_location* loc) {
    struct shard_value arg = shard_eval_lazy2(e, *args);
    if(arg.type != SHARD_VAL_INT)
        shard_eval_throw(e, *loc, "`geode.errno.toString` expects argument to be of type `integer`");

    const char* strerr = strerror(arg.integer);
    assert(strerr != NULL && "strerror() should never return NULL");

    return (struct shard_value){.type=SHARD_VAL_STRING, .string=strerr, .strlen=strlen(strerr)};
}

static struct shard_value builtin_error_throw(volatile struct shard_evaluator* e, struct shard_lazy_value** args, struct shard_location* loc) {
    struct shard_value msg = shard_eval_lazy2(e, *args);
    if(msg.type != SHARD_VAL_STRING)
        shard_eval_throw(e, *loc, "`geode.error.throe` expects argument to be of type `string`");

    struct geode_context* ctx = e->ctx->userp;

    struct shard_error* err = geode_malloc(ctx, sizeof(struct shard_error));
    *err = (struct shard_error){
        .err = geode_strdup(ctx, msg.string),
        .loc = *loc,
        .heap = false,
        ._errno = 0
    };

    geode_throw(ctx, SHARD, .shard=TUPLE(.num=1,.errs=err));
}

static struct shard_value builtin_proc_spawn(volatile struct shard_evaluator* e, struct shard_lazy_value** args, struct shard_location* loc) {
    struct shard_value exec = shard_eval_lazy2(e, args[0]);
    if(exec.type != SHARD_VAL_STRING)
        shard_eval_throw(e, *loc, "`geode.proc.spawn` expects first argument to be of type `string`");

    struct shard_value arg_list = shard_eval_lazy2(e, args[1]);
    if(arg_list.type != SHARD_VAL_LIST)
        shard_eval_throw(e, *loc, "`geode.proc.spawn` expects second argument to be of type `list`, got %d", exec.type);

    struct shard_value do_wait = shard_eval_lazy2(e, args[2]);
    if(do_wait.type != SHARD_VAL_BOOL)
        shard_eval_throw(e, *loc, "`geode.proc.spawn` expects third argument to be of type `bool`");

    size_t argc = shard_list_length(arg_list.list.head) + 1;
    char** argv = malloc(sizeof(char*) * (argc + 1));

    argv[0] = (char*) exec.string;

    size_t counter = 1;
    for(struct shard_list* arg = arg_list.list.head; arg; arg = arg->next) {
        struct shard_value value = shard_eval_lazy2(e, arg->value);
        if(value.type != SHARD_VAL_STRING)
            shard_eval_throw(e, *loc, "`geode.proc.spawn` expects second argument to be a list of strings, element `%zu` is off.", counter - 1);

        argv[counter++] = (char*) value.string;
    }

    assert(counter == argc);
    argv[argc] = NULL;

    struct geode_context* ctx = e->ctx->userp;
    if(ctx->flags.verbose) {
        infof(ctx, "proc.spawn: ");
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
            infof(ctx, "proc.spawn: '%s' terminated with signal %s (%d)\n", exec.string, strsignal(WTERMSIG(pid_status)), WTERMSIG(pid_status));
        else if(WIFSTOPPED(pid_status))
            infof(ctx, "proc.spawn: '%s' terminated with signal %s (%d)\n", exec.string, strsignal(WSTOPSIG(pid_status)), WSTOPSIG(pid_status));
    }
    else
        return_val.integer = pid;

    free(argv);

    return return_val;
}

static struct shard_value builtin_proc_spawnPipe(volatile struct shard_evaluator* e, struct shard_lazy_value** args, struct shard_location* loc) {
    struct shard_value exec = shard_eval_lazy2(e, args[0]);
    if(exec.type != SHARD_VAL_STRING)
        shard_eval_throw(e, *loc, "`geode.proc.spawnPipe` expects argument to be of type `string`");

    struct shard_value arg_list = shard_eval_lazy2(e, args[1]);
    if(arg_list.type != SHARD_VAL_LIST)
        shard_eval_throw(e, *loc, "`geode.proc.spawnPipe` expects second argument to be of type `list`, got %d", exec.type);

    struct shard_value input_string = shard_eval_lazy2(e, args[2]);
    if(input_string.type != SHARD_VAL_STRING)
        shard_eval_throw(e, *loc, "`geode.proc.spawnPipe` expects third argument to be of type `string`");

    size_t argc = shard_list_length(arg_list.list.head) + 1;
    char** argv = malloc(sizeof(char*) * (argc + 1));

    argv[0] = (char*) exec.string;

    size_t counter = 1;
    for(struct shard_list* arg = arg_list.list.head; arg; arg = arg->next) {
        struct shard_value value = shard_eval_lazy2(e, arg->value);
        if(value.type != SHARD_VAL_STRING)
            shard_eval_throw(e, *loc, "`geode.proc.spawnPipe` expects second argument to be a list of strings, element `%zu` is off.", counter - 1);

        argv[counter++] = (char*) value.string;
    }

    assert(counter == argc);
    argv[argc] = NULL;

    struct geode_context* ctx = e->ctx->userp;
    if(ctx->flags.verbose) {
        infof(ctx, "proc.spawn: ");
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

        write(stdin_pipe[1], input_string.string, input_string.strlen);
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
            infof(ctx, "proc.spawn: '%s' terminated with signal %s (%d)\n", exec.string, strsignal(WTERMSIG(pid_status)), WTERMSIG(pid_status));
        else if(WIFSTOPPED(pid_status))
            infof(ctx, "proc.spawn: '%s' terminated with signal %s (%d)\n", exec.string, strsignal(WSTOPSIG(pid_status)), WSTOPSIG(pid_status));
    } 

    struct shard_set* result = shard_set_init(e->ctx, 3);

    shard_set_put(result, shard_get_ident(e->ctx, "exitCode"), shard_unlazy(e->ctx, (struct shard_value){.type=SHARD_VAL_INT, .integer=exit_code}));
    shard_set_put(result, shard_get_ident(e->ctx, "stdout"), shard_unlazy(e->ctx, (struct shard_value){.type=SHARD_VAL_STRING, .string=out.items, .strlen=out.count - 1}));
    shard_set_put(result, shard_get_ident(e->ctx, "stderr"), shard_unlazy(e->ctx, (struct shard_value){.type=SHARD_VAL_STRING, .string=err.items, .strlen=err.count - 1}));
    
    free(argv);

    return (struct shard_value){.type=SHARD_VAL_SET, .set=result};
}
