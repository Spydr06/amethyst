#define _LIBSHARD_INTERNAL

#include "libshard.h"
#include "libshard-internal.h"

#include <errno.h>

#ifdef SHARD_ENABLE_FFI

#include <ffi.h>

#include <alloca.h>
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define PRIMITIVE_CTYPE(c, t, ffi) gen_cType((c), #t, (ffi), 0)

#define FFI_OPS(n, m) ((struct ffi_type_ops){.name = #n, .read = ffi_read_##m, .write = ffi_write_##m})

#define FFI_BUILTIN(name, cname, ...)                                                                                                                   \
        static struct shard_value builtin_##cname(volatile struct shard_evaluator* e, struct shard_builtin* builtin, struct shard_lazy_value** args);   \
        struct shard_builtin ffi_builtin_##cname = SHARD_BUILTIN(name, builtin_##cname, __VA_ARGS__)

#define ALIGN_TO(x, a) (((x) + ((a) - 1)) & ~((a) - 1))

struct ffi_type_ops {
    const char* name;
    struct shard_value (*read)(volatile struct shard_evaluator* e, const void* address, struct shard_set* ffi_type);
    void (*write)(volatile struct shard_evaluator* e, void* address, struct shard_value value, struct shard_set* ffi_type);
};

FFI_BUILTIN("builtins.ffi.void.__toString", void_toString, SHARD_VAL_SET);
FFI_BUILTIN("builtins.ffi.cPointer", cPointer, SHARD_VAL_SET);
FFI_BUILTIN("builtins.ffi.cUnion", cUnion, SHARD_VAL_SET);
FFI_BUILTIN("builtins.ffi.cStruct", cStruct, SHARD_VAL_SET);
FFI_BUILTIN("builtins.ffi.cFunc", cFunc, SHARD_VAL_SET, SHARD_VAL_LIST);
FFI_BUILTIN("builtins.ffi.cVarFunc", cVarFunc, SHARD_VAL_SET, SHARD_VAL_LIST);
FFI_BUILTIN("builtins.ffi.cCall", cCall, SHARD_VAL_SET);
FFI_BUILTIN("builtins.ffi.cValue", cValue, SHARD_VAL_SET);
FFI_BUILTIN("builtins.ffi.cAssign", cAssign, SHARD_VAL_SET, SHARD_VAL_ANY);

static struct shard_builtin* ffi_builtins[] = {
    &ffi_builtin_cPointer,
    &ffi_builtin_cFunc,
    &ffi_builtin_cVarFunc,
    &ffi_builtin_cUnion,
    &ffi_builtin_cStruct,
    &ffi_builtin_cValue,
    &ffi_builtin_cAssign,
    &ffi_builtin_cCall,
};

struct shard_hashmap ffi_type_ops;
struct shard_set* ffi_void;

static int c_typeal(volatile struct shard_evaluator* e, struct shard_set* ffi_type);
static int c_typesz(volatile struct shard_evaluator* e, struct shard_set* ffi_type);
static const char* c_typename(volatile struct shard_evaluator* e, struct shard_set* ffi_type);
static struct shard_set* c_basetype(volatile struct shard_evaluator* e, struct shard_set* ffi_type);

struct shard_value ffi_read_void(volatile struct shard_evaluator* e, const void* address, struct shard_set* ffi_type) {
    (void) address;
    (void) ffi_type;
    shard_eval_throw(e, e->error_scope->loc, "cannot read from ffi symbol with type `void`");
}

static void ffi_write_void(volatile struct shard_evaluator* e, void* address, struct shard_value value, struct shard_set* ffi_type) {
    (void) address;
    (void) value;
    (void) ffi_type;
    shard_eval_throw(e, e->error_scope->loc, "cannot write to ffi symbol with type `void`");
}

struct shard_value ffi_read_bool(volatile struct shard_evaluator* e, const void* address, struct shard_set* ffi_type) {
    (void) e;
    (void) ffi_type;
    return BOOL_VAL(*(const bool*) address);
}

static void ffi_write_bool(volatile struct shard_evaluator* e, void* address, struct shard_value value, struct shard_set* ffi_type) {
    (void) ffi_type;

    if(value.type != SHARD_VAL_BOOL)
        shard_eval_throw(e, e->error_scope->loc, "cannot write `%s` value to ffi symbol with type `bool`",
                shard_value_type_to_string(e->ctx, value.type));

    *(bool*) address = value.boolean;
}

struct shard_value ffi_read_char(volatile struct shard_evaluator* e, const void* address, struct shard_set* ffi_type) {
    (void) e;
    (void) ffi_type;
    return INT_VAL(*(const char*) address);
}

static void ffi_write_char(volatile struct shard_evaluator* e, void* address, struct shard_value value, struct shard_set* ffi_type) {
    (void) ffi_type;

    if(value.type != SHARD_VAL_INT)
        shard_eval_throw(e, e->error_scope->loc, "cannot write `%s` value to ffi symbol with type `char`",
                shard_value_type_to_string(e->ctx, value.type));

    *(char*) address = (char) value.integer;
}

struct shard_value ffi_read_short(volatile struct shard_evaluator* e, const void* address, struct shard_set* ffi_type) {
    (void) e;
    (void) ffi_type;
    return INT_VAL(*(const short*) address);
}

static void ffi_write_short(volatile struct shard_evaluator* e, void* address, struct shard_value value, struct shard_set* ffi_type) {
    (void) ffi_type;

    if(value.type != SHARD_VAL_INT)
        shard_eval_throw(e, e->error_scope->loc, "cannot write `%s` value to ffi symbol with type `short`",
                shard_value_type_to_string(e->ctx, value.type));

    *(short*) address = (short) value.integer;
}

struct shard_value ffi_read_int(volatile struct shard_evaluator* e, const void* address, struct shard_set* ffi_type) {
    (void) e;
    (void) ffi_type;
    return INT_VAL(*(const int*) address);
}

static void ffi_write_int(volatile struct shard_evaluator* e, void* address, struct shard_value value, struct shard_set* ffi_type) {
    (void) ffi_type;

    if(value.type != SHARD_VAL_INT)
        shard_eval_throw(e, e->error_scope->loc, "cannot write `%s` value to ffi symbol with type `int`",
                shard_value_type_to_string(e->ctx, value.type));

    *(int*) address = (int) value.integer;
}

struct shard_value ffi_read_long(volatile struct shard_evaluator* e, const void* address, struct shard_set* ffi_type) {
    (void) e;
    (void) ffi_type;
    return INT_VAL(*(const long*) address);
}

static void ffi_write_long(volatile struct shard_evaluator* e, void* address, struct shard_value value, struct shard_set* ffi_type) {
    (void) ffi_type;

    if(value.type != SHARD_VAL_INT)
        shard_eval_throw(e, e->error_scope->loc, "cannot write `%s` value to ffi symbol with type `long`",
                shard_value_type_to_string(e->ctx, value.type));

    *(long*) address = value.integer;
}

struct shard_value ffi_read_longlong(volatile struct shard_evaluator* e, const void* address, struct shard_set* ffi_type) {
    (void) e;
    (void) ffi_type;
    return INT_VAL(*(const long long*) address);
}

static void ffi_write_longlong(volatile struct shard_evaluator* e, void* address, struct shard_value value, struct shard_set* ffi_type) {
    (void) ffi_type;

    if(value.type != SHARD_VAL_INT)
        shard_eval_throw(e, e->error_scope->loc, "cannot write `%s` value to ffi symbol with type `long long`",
                shard_value_type_to_string(e->ctx, value.type));

    *(long long*) address = value.integer;
}

struct shard_value ffi_read_size_t(volatile struct shard_evaluator* e, const void* address, struct shard_set* ffi_type) {
    (void) e;
    (void) ffi_type;
    return INT_VAL(*(const size_t*) address);
}

static void ffi_write_size_t(volatile struct shard_evaluator* e, void* address, struct shard_value value, struct shard_set* ffi_type) {
    (void) ffi_type;

    if(value.type != SHARD_VAL_INT)
        shard_eval_throw(e, e->error_scope->loc, "cannot write `%s` value to ffi symbol with type `size_t`",
                shard_value_type_to_string(e->ctx, value.type));

    *(size_t*) address = value.integer;
}

struct shard_value ffi_read_float(volatile struct shard_evaluator* e, const void* address, struct shard_set* ffi_type) {
    (void) e;
    (void) ffi_type;
    return FLOAT_VAL(*(const float*) address);
}

static void ffi_write_float(volatile struct shard_evaluator* e, void* address, struct shard_value value, struct shard_set* ffi_type) {
    (void) ffi_type;

    if(value.type != SHARD_VAL_FLOAT)
        shard_eval_throw(e, e->error_scope->loc, "cannot write `%s` value to ffi symbol with type `float`",
                shard_value_type_to_string(e->ctx, value.type));

    *(float*) address = (float) value.floating;
}

struct shard_value ffi_read_double(volatile struct shard_evaluator* e, const void* address, struct shard_set* ffi_type) {
    (void) e;
    (void) ffi_type;
    return FLOAT_VAL(*(const double*) address);
}

static void ffi_write_double(volatile struct shard_evaluator* e, void* address, struct shard_value value, struct shard_set* ffi_type) {
    (void) ffi_type;

    if(value.type != SHARD_VAL_FLOAT)
        shard_eval_throw(e, e->error_scope->loc, "cannot write `%s` value to ffi symbol with type `double`",
                shard_value_type_to_string(e->ctx, value.type));

    *(double*) address = value.floating;
}

struct shard_value ffi_read_pointer(volatile struct shard_evaluator* e, const void* address, struct shard_set* ffi_type) {
    (void) e;
    (void) ffi_type;
    return INT_VAL(*(void* const*) address);
}

static void ffi_write_pointer(volatile struct shard_evaluator* e, void* address, struct shard_value value, struct shard_set* ffi_type) {
    struct shard_set* ffi_base = c_basetype(e, ffi_type);
    if(!ffi_base) {
        shard_eval_throw(e, e->error_scope->loc, "ffi type `pointer` does not have a set base type");
    }

    switch(value.type) {
        case SHARD_VAL_LIST:
            const char* base_name = c_typename(e, ffi_base);
            struct ffi_type_ops* base_ops = shard_hashmap_get(&ffi_type_ops, base_name);
            if(!base_ops)
                shard_eval_throw(e, e->error_scope->loc, "unexpected ffi typename `%s`", base_name);

            struct shard_list* head = value.list.head;
            size_t length = shard_list_length(head);
            
            int base_align = c_typeal(e, ffi_base);
            assert(base_align > 0);
            int base_size = c_typesz(e, ffi_base);
            assert(base_size > 0);
            int elem_size = ALIGN_TO(base_size, base_align);

            void* elems = shard_gc_malloc(e->gc, length * (size_t) elem_size);
            for(size_t i = 0; i < length; i++, head = head->next) {
                struct shard_value elem = shard_eval_lazy2(e, head->value);
                base_ops->write(e, elems + (i * elem_size), elem, ffi_base);
            }
            
            *(void**) address = (void*) elems;
            break;
        case SHARD_VAL_STRING:
            base_size = c_typesz(e, ffi_base);
            assert(base_size > 0);

            if(base_size != sizeof(char))
                shard_eval_throw(e, e->error_scope->loc, "cannot write `string` value to pointer to ffi type `%s`", c_typename(e, ffi_base));
            *(void**) address = (void*) value.string;
            break;
        case SHARD_VAL_PATH:
            *(void**) address = (void*) value.path;
            break;
        case SHARD_VAL_INT:
            *(void**) address = (void*) value.integer;
            break;
        case SHARD_VAL_NULL:
            *(void**) address = NULL;
            break;
        default:
            shard_eval_throw(e, e->error_scope->loc, "cannot write `%s` value to ffi symbol with type `pointer`",
                shard_value_type_to_string(e->ctx, value.type));
    }
}

struct shard_value ffi_read_struct(volatile struct shard_evaluator* e, const void* address, struct shard_set* ffi_type) {
    assert(false && "unimplemented");
}

static void ffi_write_struct(volatile struct shard_evaluator* e, void* address, struct shard_value value, struct shard_set* ffi_type) {
    assert(false && "unimplemented");
}

struct ffi_type_ops ffi_pointer_ops = FFI_OPS(pointer, pointer);
struct ffi_type_ops ffi_function_ops = FFI_OPS(function, pointer);
struct ffi_type_ops ffi_struct_ops = FFI_OPS(struct, struct);

static struct shard_value gen_cType(struct shard_context* ctx, const char* name, ffi_type* type, int extra) {
    struct shard_set* set = shard_set_init(ctx, 4 + extra); 

    shard_set_put(set, shard_get_ident(ctx, "name"),  shard_unlazy(ctx, CSTRING_VAL(name)));
    shard_set_put(set, shard_get_ident(ctx, "size"),  shard_unlazy(ctx, INT_VAL(type->size)));
    shard_set_put(set, shard_get_ident(ctx, "align"), shard_unlazy(ctx, INT_VAL(type->alignment)));
    shard_set_put(set, shard_get_ident(ctx, "type"),  shard_unlazy(ctx, INT_VAL((intptr_t) type)));

    return SET_VAL(set);
}

static const char* c_typename(volatile struct shard_evaluator* e, struct shard_set* ffi_type) {
    struct shard_lazy_value* lazy_name;
    if(shard_set_get(ffi_type, shard_get_ident(e->ctx, "name"), &lazy_name))
        return NULL;

    struct shard_value name_val = shard_eval_lazy2(e, lazy_name);
    if(name_val.type != SHARD_VAL_STRING)
        return NULL;

    return name_val.string;
}

static bool c_typeis(volatile struct shard_evaluator* e, struct shard_set* ffi_type, const char* name) {
    const char* typename = c_typename(e, ffi_type);
    if(!typename)
        return false;
    return strcmp(name, typename) == 0;
}

static int c_typesz(volatile struct shard_evaluator* e, struct shard_set* ffi_type) {
    struct shard_lazy_value* lazy_size;
    if(shard_set_get(ffi_type, shard_get_ident(e->ctx, "size"), &lazy_size))
        return -1;

    struct shard_value type_val = shard_eval_lazy2(e, lazy_size);
    if(type_val.type != SHARD_VAL_INT)
        return -1;

    return type_val.integer;
}

static int c_typeal(volatile struct shard_evaluator* e, struct shard_set* ffi_type) {
    struct shard_lazy_value* lazy_align;
    if(shard_set_get(ffi_type, shard_get_ident(e->ctx, "align"), &lazy_align))
        return -1;

    struct shard_value align_val = shard_eval_lazy2(e, lazy_align);
    if(align_val.type != SHARD_VAL_INT)
        return -1;

    return align_val.integer;
}

static ffi_type *c_typeffi(volatile struct shard_evaluator *e, struct shard_set *ffi_type) {
    struct shard_lazy_value* lazy_type;
    if(shard_set_get(ffi_type, shard_get_ident(e->ctx, "type"), &lazy_type))
        return NULL;

    struct shard_value ffi_type_val = shard_eval_lazy2(e, lazy_type);
    if(ffi_type_val.type != SHARD_VAL_INT)
        return NULL;

    return (void*) ffi_type_val.integer;
}

static struct shard_set* c_basetype(volatile struct shard_evaluator* e, struct shard_set* ffi_type) {
    if(!c_typeis(e, ffi_type, "pointer"))
        return NULL;

    struct shard_lazy_value* lazy_base;
    if(shard_set_get(ffi_type, shard_get_ident(e->ctx, "base"), &lazy_base))
        return NULL;

    struct shard_value base_val = shard_eval_lazy2(e, lazy_base);
    if(base_val.type != SHARD_VAL_SET)
        return NULL;
    return base_val.set;
}

static struct shard_list* c_arglist(volatile struct shard_evaluator* e, struct shard_set* ffi_type) {
    if(!c_typeis(e, ffi_type, "function"))
        return NULL;

    struct shard_lazy_value* lazy_args;
    if(shard_set_get(ffi_type, shard_get_ident(e->ctx, "arguments"), &lazy_args))
        return NULL;

    struct shard_value arg_list = shard_eval_lazy2(e, lazy_args);
    if(arg_list.type != SHARD_VAL_LIST)
        return NULL;

    return arg_list.list.head;
}

static bool c_isvariadic(volatile struct shard_evaluator *e, struct shard_set *ffi_type) {
    if(!c_typeis(e, ffi_type, "function"))
        return false;

    struct shard_lazy_value *lazy_variadic;
    if(shard_set_get(ffi_type, shard_get_ident(e->ctx, "variadic"), &lazy_variadic))
        return false;

    struct shard_value variadic = shard_eval_lazy2(e, lazy_variadic);
    return variadic.type == SHARD_VAL_BOOL && variadic.boolean;
}

static struct shard_set* c_return(volatile struct shard_evaluator* e, struct shard_set* ffi_type) {
    if(!c_typeis(e, ffi_type, "function"))
        return NULL;

    struct shard_lazy_value* lazy_return;
    if(shard_set_get(ffi_type, shard_get_ident(e->ctx, "returns"), &lazy_return))
        return NULL;

    struct shard_value ret = shard_eval_lazy2(e, lazy_return);
    if(ret.type != SHARD_VAL_SET)
        return NULL;

    return ret.set;
}

static int c_argcnt(volatile struct shard_evaluator* e, struct shard_set* ffi_type) { 
    struct shard_list* arg_head = c_arglist(e, ffi_type);
    if(!arg_head)
        return -1;
    return (int) shard_list_length(arg_head);
}

static void* binding_address(volatile struct shard_evaluator* e, struct shard_set* binding) {
    struct shard_lazy_value* lazy_address;
    if(shard_set_get(binding, shard_get_ident(e->ctx, "address"), &lazy_address))
        return NULL;

    struct shard_value address_val = shard_eval_lazy2(e, lazy_address);
    if(address_val.type != SHARD_VAL_INT)
        return NULL;

    return (void*) address_val.integer;
}

static struct shard_set* binding_ffi_type(volatile struct shard_evaluator* e, struct shard_set* binding) {
    struct shard_lazy_value* lazy_type;
    if(shard_set_get(binding, shard_get_ident(e->ctx, "type"), &lazy_type))
        return NULL;

    struct shard_value type_val = shard_eval_lazy2(e, lazy_type);
    if(type_val.type != SHARD_VAL_SET)
        return NULL;

    return type_val.set;
}

SHARD_DECL int shard_enable_ffi(struct shard_context* ctx) {
    int err;

    shard_hashmap_init(ctx, &ffi_type_ops, 32);

    shard_hashmap_put(ctx, &ffi_type_ops, "pointer", &ffi_pointer_ops);
    shard_hashmap_put(ctx, &ffi_type_ops, "function", &ffi_function_ops);
    shard_hashmap_put(ctx, &ffi_type_ops, "struct", &ffi_struct_ops);

    ffi_void = shard_set_init(ctx, 1);
    shard_set_put(ffi_void, shard_get_ident(ctx, "__toString"), shard_unlazy(ctx, BUILTIN_VAL(&ffi_builtin_void_toString)));

    struct {
        const char* ident;
        struct shard_value value;
        struct ffi_type_ops ops;
    } ffi_constants[] = {
        {"builtins.ffi.void", SET_VAL(ffi_void), {NULL, NULL, NULL, NULL, NULL}},

        {"builtins.ffi.cVoid", PRIMITIVE_CTYPE(ctx, void, &ffi_type_void), FFI_OPS(void, void)},
        {"builtins.ffi.cBool", PRIMITIVE_CTYPE(ctx, bool, &ffi_type_uint8), FFI_OPS(bool, bool)},

        {"builtins.ffi.cChar", PRIMITIVE_CTYPE(ctx, char, &ffi_type_schar), FFI_OPS(char, char)},
        {"builtins.ffi.cShort", PRIMITIVE_CTYPE(ctx, short, &ffi_type_sshort), FFI_OPS(short, short)},
        {"builtins.ffi.cInt", PRIMITIVE_CTYPE(ctx, int, &ffi_type_sint), FFI_OPS(int, int)},
        {"builtins.ffi.cLong", PRIMITIVE_CTYPE(ctx, long, &ffi_type_slong), FFI_OPS(long, long)},

        {"builtins.ffi.cUnsignedChar", PRIMITIVE_CTYPE(ctx, unsigned char, &ffi_type_uchar), FFI_OPS(unsigned char, char)},
        {"builtins.ffi.cUnsignedShort", PRIMITIVE_CTYPE(ctx, unsigned short, &ffi_type_ushort), FFI_OPS(unsigned short, short)},
        {"builtins.ffi.cUnsignedInt", PRIMITIVE_CTYPE(ctx, unsigned int, &ffi_type_uint), FFI_OPS(unsigned int, int)},
        {"builtins.ffi.cUnsignedLong", PRIMITIVE_CTYPE(ctx, unsigned long, &ffi_type_ulong), FFI_OPS(unsigned long, long)},

        {"builtins.ffi.cFloat", PRIMITIVE_CTYPE(ctx, float, &ffi_type_float), FFI_OPS(float, float)},
        {"builtins.ffi.cDouble", PRIMITIVE_CTYPE(ctx, double, &ffi_type_double), FFI_OPS(double, double)},

        {"builtins.ffi.cSizeT", PRIMITIVE_CTYPE(ctx, size_t, &ffi_type_uint64), FFI_OPS(size_t, size_t)},
    };

    for(size_t i = 0; i < LEN(ffi_constants); i++) {
        err = shard_define_constant(
            ctx,
            ffi_constants[i].ident,
            ffi_constants[i].value
        );

        if(err)
            return err;

        if(ffi_constants[i].ops.name) {
            struct ffi_type_ops* ffi_ops = shard_gc_malloc(ctx->gc, sizeof(struct ffi_type_ops));
            shard_gc_make_static(ctx->gc, ffi_ops);
            memcpy(ffi_ops, &ffi_constants[i].ops, sizeof(struct ffi_type_ops));
            shard_hashmap_put(ctx, &ffi_type_ops, ffi_constants[i].ops.name, ffi_ops);
        }
    }

    for(size_t i = 0; i < sizeof(ffi_builtins) / sizeof(void*); i++) {
        if((err = shard_define_builtin(ctx, ffi_builtins[i])))
            break;
    }

    return 0;
}

SHARD_DECL struct shard_value shard_ffi_bind(volatile struct shard_evaluator* e, const char* symbol_name, void* symbol_address, struct shard_set* ffi_type) {
    bool is_function = c_typeis(e, ffi_type, "function");

    struct shard_set* binding = shard_set_init(e->ctx, 3 + (int) is_function);

    shard_set_put(binding, shard_get_ident(e->ctx, "name"), shard_unlazy(e->ctx, CSTRING_VAL(symbol_name)));
    shard_set_put(binding, shard_get_ident(e->ctx, "address"), shard_unlazy(e->ctx, INT_VAL((intptr_t) symbol_address)));
    shard_set_put(binding, shard_get_ident(e->ctx, "type"), shard_unlazy(e->ctx, SET_VAL(ffi_type)));

    if(is_function) {
        size_t arg_count = c_argcnt(e, ffi_type);

        struct shard_value call_value;
        call_value.type = SHARD_VAL_BUILTIN;
        call_value.builtin.builtin = shard_gc_malloc(e->gc, sizeof(struct shard_builtin));
        memcpy(call_value.builtin.builtin, &ffi_builtin_cCall, sizeof(struct shard_builtin));

        call_value.builtin.queued_args = NULL;
        call_value.builtin.num_queued_args = 0;

        call_value.builtin.builtin->arity += arg_count;

        shard_set_put(binding, shard_get_ident(e->ctx, "__functor"), shard_unlazy(e->ctx, call_value));
    }

    return SET_VAL(binding);
}

struct shard_value ffi_c_to_shard(volatile struct shard_evaluator* e, const void* symbol_address, struct shard_set* ffi_type) {
    const char* name = c_typename(e, ffi_type);
    assert(name != NULL);

    struct ffi_type_ops* ops = shard_hashmap_get(&ffi_type_ops, name);
    if(!ops)
        shard_eval_throw(e, e->error_scope->loc, "unexpected ffi typename `%s`", name);

    return ops->read(e, symbol_address, ffi_type);
}

void ffi_shard_to_c(volatile struct shard_evaluator* e, struct shard_value value, void* symbol_address, struct shard_set *ffi_type) {
    const char* name = c_typename(e, ffi_type);
    assert(name != NULL);

    struct ffi_type_ops* ops = shard_hashmap_get(&ffi_type_ops, name);
    if(!ops)
        shard_eval_throw(e, e->error_scope->loc, "unexpected ffi typename `%s`", name);

    ops->write(e, symbol_address, value, ffi_type);
}

static struct shard_value builtin_void_toString(volatile struct shard_evaluator*, struct shard_builtin*, struct shard_lazy_value**) {
    return CSTRING_VAL("void");
}

static struct shard_value builtin_cPointer(volatile struct shard_evaluator* e, struct shard_builtin* builtin, struct shard_lazy_value** args) {
    struct shard_value type = gen_cType(e->ctx, "pointer", &ffi_type_pointer, 1); 

    shard_builtin_eval_arg(e, builtin, args, 0);
    shard_set_put(type.set, shard_get_ident(e->ctx, "base"), args[0]);

    return type;
}

static struct shard_value builtin_cFunc(volatile struct shard_evaluator* e, struct shard_builtin* builtin, struct shard_lazy_value** args) {
    struct shard_value type = gen_cType(e->ctx, "function", &ffi_type_pointer, 2);

    shard_builtin_eval_arg(e, builtin, args, 0);
    shard_set_put(type.set, shard_get_ident(e->ctx, "returns"), args[0]);

    shard_builtin_eval_arg(e, builtin, args, 1);
    shard_set_put(type.set, shard_get_ident(e->ctx, "arguments"), args[1]);

    return type;
}

static struct shard_value builtin_cVarFunc(volatile struct shard_evaluator* e, struct shard_builtin* builtin, struct shard_lazy_value** args) {
    struct shard_value type = gen_cType(e->ctx, "function", &ffi_type_pointer, 3);

    shard_builtin_eval_arg(e, builtin, args, 0);
    shard_set_put(type.set, shard_get_ident(e->ctx, "returns"), args[0]);

    shard_builtin_eval_arg(e, builtin, args, 1);
    shard_set_put(type.set, shard_get_ident(e->ctx, "arguments"), args[1]);

    struct shard_lazy_value *variadic = shard_unlazy(e->ctx, TRUE_VAL());
    shard_set_put(type.set, shard_get_ident(e->ctx, "variadic"), variadic);

    return type;
}

static struct shard_value builtin_cStruct(volatile struct shard_evaluator* e, struct shard_builtin* builtin, struct shard_lazy_value** args) {
    struct shard_value fields = shard_builtin_eval_arg(e, builtin, args, 0);
    size_t num_fields = 0;
    for(size_t i = 0; i < fields.set->capacity; i++) {
        if(!fields.set->entries[i].key)
            continue;
        num_fields++;
    }

    ffi_type **ffi_fields = shard_gc_malloc(e->gc, sizeof(ffi_type*) * (num_fields + 1));

    for(size_t i = 0; i < fields.set->capacity; i++) {
        if(!fields.set->entries[i].key)
            continue;

        struct shard_value field_val = shard_eval_lazy2(e, fields.set->entries[i].value);
        if(field_val.type != SHARD_VAL_SET)
            shard_eval_throw(e, e->error_scope->loc, "field `%s` passed to `builtins.ffi.cStruct` is not a ffi type", fields.set->entries[i].key);    

        ffi_type *field_ffi = c_typeffi(e, field_val.set);
        if(!field_ffi)
            shard_eval_throw(e, e->error_scope->loc, "field `%s` passed to `builtins.ffi.cStruct` is missing `type` attr", fields.set->entries[i].key);

        ffi_fields[i] = field_ffi;
        
        int field_size = c_typesz(e, field_val.set);
        if(field_size < 0)
            shard_eval_throw(e, e->error_scope->loc, "field `%s` passed to `builtins.ffi.cStruct` is missing `size` attr", fields.set->entries[i].key);

        int field_align = c_typeal(e, field_val.set);
        if(field_align < 0)
            shard_eval_throw(e, e->error_scope->loc, "field `%s` passed to `builtins.ffi.cStruct` is missing `align` attr", fields.set->entries[i].key);
    }


    ffi_type *ffi_struct = shard_gc_calloc(e->gc, 1, sizeof(ffi_type));
    ffi_struct->type = FFI_TYPE_STRUCT;
    ffi_struct->elements = ffi_fields;
    ffi_struct->elements[num_fields] = NULL;

    // initializes ffi_struct->alignment and ffi_struct->size
    ffi_cif cif_prop;
    ffi_prep_cif(&cif_prop, FFI_DEFAULT_ABI, 1, &ffi_type_void, &ffi_struct);

    struct shard_value type = gen_cType(e->ctx, "struct", ffi_struct, 1); 
    shard_set_put(type.set, shard_get_ident(e->ctx, "fields"), args[0]);

    return type;
}

static struct shard_value builtin_cUnion(volatile struct shard_evaluator* e, struct shard_builtin* builtin, struct shard_lazy_value** args) {
    struct shard_value fields = shard_builtin_eval_arg(e, builtin, args, 0);
    int size = 0;
    int align = 0;

    ffi_type *biggest_field = NULL;

    for(size_t i = 0; i < fields.set->capacity; i++) {
        if(!fields.set->entries[i].key)
            continue;

        struct shard_value field_val = shard_eval_lazy2(e, fields.set->entries[i].value);
        if(field_val.type != SHARD_VAL_SET)
            shard_eval_throw(e, e->error_scope->loc, "field `%s` passed to `builtins.ffi.cUnion` is not a ffi type", fields.set->entries[i].key);    

        ffi_type *field_ffi = c_typeffi(e, field_val.set);
        if(!field_ffi)
            shard_eval_throw(e, e->error_scope->loc, "field `%s` passed to `builtins.ffi.cStruct` is missing `type` attr", fields.set->entries[i].key);

        int field_size = c_typesz(e, field_val.set);
        if(field_size < 0)
            shard_eval_throw(e, e->error_scope->loc, "field `%s` passed to `builtins.ffi.cStruct` is missing `size` attr", fields.set->entries[i].key);

        int field_align = c_typeal(e, field_val.set);
        if(field_align < 0)
            shard_eval_throw(e, e->error_scope->loc, "field `%s` passed to `builtins.ffi.cStruct` is missing `align` attr", fields.set->entries[i].key);
    
        if(field_size > size) {
            size = field_size;
            biggest_field = field_ffi;
        }

        align = MAX(field_align, align);
    }

    ffi_type **ffi_fields = shard_gc_malloc(e->gc, sizeof(ffi_type*) * 2);
    ffi_fields[0] = biggest_field;
    ffi_fields[1] = NULL;

    ffi_type *ffi_struct = shard_gc_calloc(e->gc, 1, sizeof(ffi_type));
    ffi_struct->type = FFI_TYPE_STRUCT;
    ffi_struct->elements = ffi_fields;
    ffi_struct->alignment = align;
    ffi_struct->size = size;

    struct shard_value type = gen_cType(e->ctx, "union", ffi_struct, 1); 
    shard_set_put(type.set, shard_get_ident(e->ctx, "fields"), args[0]);

    return type;
}

static struct shard_value builtin_cCall(volatile struct shard_evaluator* e, struct shard_builtin* builtin, struct shard_lazy_value** args) {
    struct shard_value binding = shard_builtin_eval_arg(e, builtin, args, 0);

    void* address = binding_address(e, binding.set);
    if(!address)
        shard_eval_throw(e, e->error_scope->loc, "invalid or missing attr `address` of binding passed to `builtins.ffi.cCall`");

    struct shard_set* func_type = binding_ffi_type(e, binding.set);
    if(!func_type)
        shard_eval_throw(e, e->error_scope->loc, "invalid or missing attr `type` of binding passed to `builtins.ffi.cCall`");

    if(!c_typeis(e, func_type, "function"))
        shard_eval_throw(e, e->error_scope->loc, "`builtins.ffi.cCall` can only call ffi bindings of type `function`");

    struct shard_list* arg_type_list = c_arglist(e, func_type);
    size_t arg_count = shard_list_length(arg_type_list);

    struct shard_set* return_type = c_return(e, func_type);
    if(!return_type)
        shard_eval_throw(e, e->error_scope->loc, "could not get return type from ffi binding passed to `builtins.ffi.cCall`");

    size_t return_type_size = c_typesz(e, return_type);
    uint8_t ffi_return_buffer[MAX(return_type_size, sizeof(size_t))];

    ffi_type *ffi_return_type = c_typeffi(e, return_type);
    if(!ffi_return_type)
        shard_eval_throw(e, e->error_scope->loc, "ffi return type has missing attr `type`");

    ffi_type *ffi_arg_types[arg_count];
    void *ffi_arg_buffers[arg_count];

    for(size_t i = 0; i < arg_count; i++) {
        assert(arg_type_list != NULL);

        struct shard_value arg_type = shard_eval_lazy2(e, arg_type_list->value);
        arg_type_list = arg_type_list->next;

        if(arg_type.type != SHARD_VAL_SET)
            shard_eval_throw(e, e->error_scope->loc, 
                "argument `%zu` of ffi type `function` is not of type `set`, got `%s`", 
                i,
                shard_value_type_to_string(e->ctx, arg_type.type)
            );

        struct shard_value arg_val = shard_eval_lazy2(e, args[i + 1]);
        
        ffi_arg_types[i] = c_typeffi(e, arg_type.set);
        if(!ffi_arg_types[i])
            shard_eval_throw(e, e->error_scope->loc, "argument `%zu` of ffi type `function` has missing attr `type`", i);
        
        size_t arg_size = c_typesz(e, arg_type.set);
        ffi_arg_buffers[i] = alloca(MAX(arg_size, sizeof(size_t)));

        ffi_shard_to_c(e, arg_val, ffi_arg_buffers[i], arg_type.set);
    }

    ffi_cif cif;
    ffi_status err;

    if((err = ffi_prep_cif(&cif, FFI_DEFAULT_ABI, arg_count, ffi_return_type, ffi_arg_types)) != FFI_OK)
        shard_eval_throw(e, e->error_scope->loc, "internal error: ffi_prep_cif() failed");

    ffi_call(&cif, address, ffi_return_buffer, ffi_arg_buffers);

    if(ffi_return_type->type == FFI_TYPE_VOID)
        return SET_VAL(ffi_void);
    return ffi_c_to_shard(e, ffi_return_buffer, return_type);
}

static struct shard_value builtin_cValue(volatile struct shard_evaluator* e, struct shard_builtin* builtin, struct shard_lazy_value** args) {
    struct shard_value binding = shard_builtin_eval_arg(e, builtin, args, 0);
   
    void* address = binding_address(e, binding.set);
    if(!address)
        shard_eval_throw(e, e->error_scope->loc, "invalid or missing attr `address` of binding passed to `builtins.ffi.cValue`");
   
    struct shard_set* ffi_type = binding_ffi_type(e, binding.set);
    if(!ffi_type)
        shard_eval_throw(e, e->error_scope->loc, "invalid or missing attr `type` of binding passed to `builtins.ffi.cValue`");
    
    return ffi_c_to_shard(e, address, ffi_type);
}

static struct shard_value builtin_cAssign(volatile struct shard_evaluator* e, struct shard_builtin* builtin, struct shard_lazy_value** args) {
    struct shard_value binding = shard_builtin_eval_arg(e, builtin, args, 0);
    struct shard_value value = shard_builtin_eval_arg(e, builtin, args, 1);

    void* address = binding_address(e, binding.set);
    if(!address)
        shard_eval_throw(e, e->error_scope->loc, "invalid or missing attr `address` of binding passed to `builtins.ffi.cAssign`");

    struct shard_set* ffi_type = binding_ffi_type(e, binding.set);
    if(!ffi_type)
        shard_eval_throw(e, e->error_scope->loc, "invalid or missing attr `type` of binding passed to `builtins.ffi.cAssign`");

    ffi_shard_to_c(e, value, address, ffi_type);
    return value;
}

#else

SHARD_DECL int shard_enable_ffi(struct shard_context *) {
    return EINVAL;
}

#endif

