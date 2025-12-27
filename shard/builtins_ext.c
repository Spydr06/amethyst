#define _DEFAULT_SOURCE
#include "shard.h"

#include <assert.h>

#ifndef _SHARD_NO_FFI
#include <dlfcn.h>
#endif

#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <dirent.h>
#include <sys/stat.h>

#include <libshard.h>

#define QUOTE(_x) #_x

#define NULL_VAL() ((struct shard_value) { .type = SHARD_VAL_NULL })
#define INT_VAL(i) ((struct shard_value) { .type = SHARD_VAL_INT, .integer = (int64_t)(i) })

#define STRING_VAL(s, l) ((struct shard_value) { .type = SHARD_VAL_STRING, .string = (s), .strlen = (l) })
#define CSTRING_VAL(s) STRING_VAL(s, strlen((s)))
#define SET_VAL(_set) ((struct shard_value) { .type = SHARD_VAL_SET, .set = (_set) })

#define EXT_BUILTIN(name, cname, ...)                                                                                                                   \
        static struct shard_value builtin_##cname(volatile struct shard_evaluator* e, struct shard_builtin* builtin, struct shard_lazy_value** args);   \
        struct shard_builtin ext_builtin_##cname = SHARD_BUILTIN(name, builtin_##cname, __VA_ARGS__)

EXT_BUILTIN("system.getEnv", getEnv, SHARD_VAL_STRING);
EXT_BUILTIN("system.setEnv", setEnv, SHARD_VAL_STRING, SHARD_VAL_STRING);
EXT_BUILTIN("system.dlOpen", dlOpen, SHARD_VAL_TEXTUAL | SHARD_VAL_NULL);
EXT_BUILTIN("system.dlClose", dlClose, SHARD_VAL_SET);
EXT_BUILTIN("system.dlSym", dlSym, SHARD_VAL_SET, SHARD_VAL_STRING, SHARD_VAL_SET);
EXT_BUILTIN("system.exit", exit, SHARD_VAL_INT);
EXT_BUILTIN("system.printLn", printLn, SHARD_VAL_STRING);
EXT_BUILTIN("system.readFile", readFile, SHARD_VAL_PATH);
EXT_BUILTIN("system.writeFile", writeFile, SHARD_VAL_PATH, SHARD_VAL_STRING);
EXT_BUILTIN("system.readDir", readDir, SHARD_VAL_PATH);
EXT_BUILTIN("system.stat", stat, SHARD_VAL_PATH);
EXT_BUILTIN("system.exists", exists, SHARD_VAL_PATH);

static struct shard_builtin* ext_builtins[] = {
    &ext_builtin_getEnv,
    &ext_builtin_setEnv,
    &ext_builtin_dlOpen,
    &ext_builtin_dlClose,
    &ext_builtin_dlSym,
    &ext_builtin_exit,
    &ext_builtin_printLn,
    &ext_builtin_readFile,
    &ext_builtin_writeFile,
    &ext_builtin_readDir,
    &ext_builtin_stat,
    &ext_builtin_exists,
};

static struct shard_value gen_arg_list(struct shard_context* ctx, int argc, char** argv) {
    struct shard_value list;
    list.type = SHARD_VAL_LIST;
    list.list.head = NULL;

    struct shard_list** head = &list.list.head;

    for(int i = 0; i < argc; i++) {
        struct shard_value arg = CSTRING_VAL(argv[i]);
        
        *head = shard_gc_malloc(ctx->gc, sizeof(struct shard_list));
        (*head)->value = shard_unlazy(ctx, arg);
        (*head)->next = NULL;
        head = &(*head)->next;
    }

    return list;
}

int load_constants(struct shard_context* ctx, int argc, char** argv) {
    struct {
        const char* ident;
        struct shard_value value;
    } builtin_constants[] = {
        {"system.getArgs", gen_arg_list(ctx, argc, argv)},
        {"system.exitSuccess", INT_VAL(EXIT_SUCCESS)},
        {"system.exitFailure", INT_VAL(EXIT_FAILURE)},
    };

    for(size_t i = 0; i < __len(builtin_constants); i++) {
        int err = shard_define_constant(
            ctx,
            builtin_constants[i].ident,
            builtin_constants[i].value
        );

        if(err)
            return err;
    }

    return 0;
}

int load_ext_builtins(struct shard_context* ctx, int argc, char** argv) {
    int err = load_constants(ctx, argc, argv);
    if(err)
        return err;

#ifndef _SHARD_NO_FFI
    if((err = shard_enable_ffi(ctx)))
        return err;
#endif

    for(size_t i = 0; i < sizeof(ext_builtins) / sizeof(void*); i++) {
        if((err = shard_define_builtin(ctx, ext_builtins[i])))
            break;
    }

    return err;
}

static struct shard_value builtin_getEnv(volatile struct shard_evaluator* e, struct shard_builtin* builtin, struct shard_lazy_value** args) {
    struct shard_value name = shard_builtin_eval_arg(e, builtin, args, 0);

    char* value = getenv(name.string);
    if(!value)
        return NULL_VAL();
    return CSTRING_VAL(value);
}

static struct shard_value builtin_setEnv(volatile struct shard_evaluator* e, struct shard_builtin* builtin, struct shard_lazy_value** args) {
    struct shard_value name = shard_builtin_eval_arg(e, builtin, args, 0);
    struct shard_value value = shard_builtin_eval_arg(e, builtin, args, 1);

    int err = setenv(name.string, value.string, 1);
    if(err < 0)
        return INT_VAL(errno);
    return INT_VAL(0);
}

static struct shard_value builtin_dlOpen(volatile struct shard_evaluator* e, struct shard_builtin* builtin, struct shard_lazy_value** args) {
#ifndef _SHARD_NO_FFI
    struct shard_value path = shard_builtin_eval_arg(e, builtin, args, 0);

    const char *dlpath = path.type == SHARD_VAL_NULL ? NULL : path.path;
    void* handle = dlopen(dlpath, RTLD_LAZY);
    if(!handle)
        shard_eval_throw(e, e->error_scope->loc, "dlopen: %s", dlerror());
    
    struct shard_set* dylib = shard_set_init(e->ctx, 2);

    shard_set_put(dylib, shard_get_ident(e->ctx, "filename"), args[0]);
    if(handle) {
        shard_set_put(dylib, shard_get_ident(e->ctx, "handle"), shard_unlazy(e->ctx, INT_VAL((intptr_t) handle)));
    }
    else {
        char* err = dlerror();
        err = shard_gc_strdup(e->gc, err, strlen(err));
        shard_set_put(dylib, shard_get_ident(e->ctx, "error"), shard_unlazy(e->ctx, CSTRING_VAL(err)));
    }

    return SET_VAL(dylib);
#else
    return NULL_VAL();
#endif
}

static struct shard_value builtin_dlClose(volatile struct shard_evaluator* e, struct shard_builtin* builtin, struct shard_lazy_value** args) {
#ifndef _SHARD_NO_FFI 
    struct shard_value dylib = shard_builtin_eval_arg(e, builtin, args, 0);

    int err;
    struct shard_lazy_value* lazy_handle;
    
    if((err = shard_set_get(dylib.set, shard_get_ident(e->ctx, "handle"), &lazy_handle)))
        return INT_VAL(err);

    struct shard_value handle_val = shard_eval_lazy2(e, lazy_handle);
    if(handle_val.type != SHARD_VAL_INT)
        return INT_VAL(EINVAL);

    void* handle = (void*) handle_val.integer;
    return INT_VAL(dlclose(handle));
#else
    return NULL_VAL();
#endif
}

static struct shard_value builtin_dlSym(volatile struct shard_evaluator* e, struct shard_builtin* builtin, struct shard_lazy_value** args) {
#ifndef _SHARD_NO_FFI
    struct shard_value dylib_val = shard_builtin_eval_arg(e, builtin, args, 0);
    struct shard_value symbol_val = shard_builtin_eval_arg(e, builtin, args, 1);
    struct shard_value ffi_type_val = shard_builtin_eval_arg(e, builtin, args, 2);

    int err;
    struct shard_lazy_value* lazy_handle;
    if((err = shard_set_get(dylib_val.set, shard_get_ident(e->ctx, "handle"), &lazy_handle)))
        return CSTRING_VAL("missing dl attr `handle`");

    struct shard_value handle_val = shard_eval_lazy2(e, lazy_handle);
    if(handle_val.type != SHARD_VAL_INT)
        return CSTRING_VAL("invalid dl attr `handle`");

    struct shard_lazy_value* lazy_filename;
    if((err = shard_set_get(dylib_val.set, shard_get_ident(e->ctx, "filename"), &lazy_filename)))
        return CSTRING_VAL("missing dl attr `filename`");

    struct shard_value filename_val = shard_eval_lazy2(e, lazy_filename);
    if(!(filename_val.type & (SHARD_VAL_PATH | SHARD_VAL_NULL)))
        return CSTRING_VAL("invalid dl attr `filename`");

    void* handle = (void*) handle_val.integer;
    const char* symbol = symbol_val.string;

    void* sym = dlsym(handle, symbol);
    if(!sym) {
        char* err = dlerror();
        err = shard_gc_strdup(e->gc, err, strlen(err));
        return CSTRING_VAL(err);
    }

    return shard_ffi_bind(e, symbol, sym, ffi_type_val.set);
#else
    return NULL_VAL();
#endif
}

static struct shard_value builtin_exit(volatile struct shard_evaluator* e, struct shard_builtin* builtin, struct shard_lazy_value** args) {
    struct shard_value exit_code = shard_builtin_eval_arg(e, builtin, args, 0);
    exit(exit_code.integer);
}

static struct shard_value builtin_printLn(volatile struct shard_evaluator* e, struct shard_builtin* builtin, struct shard_lazy_value** args) {
    struct shard_value message = shard_builtin_eval_arg(e, builtin, args, 0);

    int err = fwrite(message.string, sizeof(char), message.strlen, stdout);
    fflush(stdout);
    return INT_VAL(err);
}

static struct shard_value builtin_exists(volatile struct shard_evaluator* e, struct shard_builtin* builtin, struct shard_lazy_value** args) {
    struct shard_value arg = shard_builtin_eval_arg(e, builtin, args, 0);
    
    return (struct shard_value){.type=SHARD_VAL_BOOL, .boolean=access(arg.path, F_OK) == 0};
}

static struct shard_value builtin_readFile(volatile struct shard_evaluator* e, struct shard_builtin* builtin, struct shard_lazy_value** args) {
    struct shard_value file = shard_builtin_eval_arg(e, builtin, args, 0);

    FILE* fp = fopen(file.path, "r");
    if(!fp)
        return (struct shard_value){.type=SHARD_VAL_INT, .integer=errno};

    fseek(fp, 0, SEEK_END);
    size_t filesz = ftell(fp);
    fseek(fp, 0, SEEK_SET);

    char *data = shard_gc_malloc(e->gc, filesz + 1);
    ssize_t bytes_read = fread(data, sizeof(char), filesz, fp);

    data[bytes_read] = '\0';

    fclose(fp);

    return (struct shard_value){.type=SHARD_VAL_STRING, .string=data, .strlen=filesz};
}

static struct shard_value builtin_writeFile(volatile struct shard_evaluator* e, struct shard_builtin* builtin, struct shard_lazy_value** args) {
    struct shard_value file = shard_builtin_eval_arg(e, builtin, args, 0);
    struct shard_value data = shard_builtin_eval_arg(e, builtin, args, 1);

    FILE* fp = fopen(file.string, "w");
    if(!fp)
        return (struct shard_value){.type=SHARD_VAL_INT, .integer=errno};

    fwrite(data.string, sizeof(char), data.strlen, fp);
    int err = fclose(fp);

    return (struct shard_value){.type=SHARD_VAL_INT, .integer=err};
}

static struct shard_value builtin_readDir(volatile struct shard_evaluator* e, struct shard_builtin* builtin, struct shard_lazy_value** args) {
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

static struct shard_value builtin_stat(volatile struct shard_evaluator* e, struct shard_builtin* builtin, struct shard_lazy_value** args) {
    struct shard_value path = shard_builtin_eval_arg(e, builtin, args, 0);

    struct stat st;
    if(stat(path.path, &st) < 0)
        return (struct shard_value){.type=SHARD_VAL_INT, .integer=errno};

    struct shard_set *st_set = shard_set_init(e->ctx, 13);

#define ST_ENTRY(_set, _st, _field) shard_set_put((_set), shard_get_ident(e->ctx, QUOTE(_field)), shard_unlazy(e->ctx, (struct shard_value){.type=SHARD_VAL_INT, .integer=(_st)._field}));

    ST_ENTRY(st_set, st, st_dev);
    ST_ENTRY(st_set, st, st_ino);
    ST_ENTRY(st_set, st, st_nlink);
    ST_ENTRY(st_set, st, st_mode);
    ST_ENTRY(st_set, st, st_uid);
    ST_ENTRY(st_set, st, st_gid);
    ST_ENTRY(st_set, st, st_rdev);
    ST_ENTRY(st_set, st, st_size);
    ST_ENTRY(st_set, st, st_blksize);
    ST_ENTRY(st_set, st, st_blocks);
    
#undef ST_ENTRY
    return SET_VAL(st_set);
}

