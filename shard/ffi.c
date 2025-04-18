#include "libshard.h"
#include "shard.h"

#include <alloca.h>
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define NULL_VAL() ((struct shard_value) { .type = SHARD_VAL_NULL })
#define BOOL_VAL(b) ((struct shard_value) { .type = SHARD_VAL_BOOL, .boolean = (bool)(b) })
#define INT_VAL(i) ((struct shard_value) { .type = SHARD_VAL_INT, .integer = (int64_t)(i) })
#define FLOAT_VAL(f) ((struct shard_value) { .type = SHARD_VAL_FLOAT, .floating = (double)(f) })
#define SET_VAL(_set) ((struct shard_value) { .type = SHARD_VAL_SET, .set = (_set) })
#define BUILTIN_VAL(b) ((struct shard_value) {.type = SHARD_VAL_BUILTIN, .builtin.builtin=(b), .builtin.queued_args=NULL, .builtin.num_queued_args=0 })
#define STRING_VAL(s, l) ((struct shard_value) { .type = SHARD_VAL_STRING, .string = (s), .strlen = (l) })
#define CSTRING_VAL(s) STRING_VAL(s, strlen((s)))

#define PRIMITIVE_CTYPE(c, t) gen_cType((c), #t, sizeof(t), _Alignof(t), 0)

#define FFI_OPS(n, m, a, r) ((struct ffi_type_ops){.name = #n, .read = ffi_read_##m, .write = ffi_write_##m, .as_ccall_arg = (a), .from_ccall_return = (r)})

#define FFI_BUILTIN(name, cname, ...)                                                                                                                   \
        static struct shard_value builtin_##cname(volatile struct shard_evaluator* e, struct shard_builtin* builtin, struct shard_lazy_value** args);   \
        struct shard_builtin ffi_builtin_##cname = SHARD_BUILTIN(name, builtin_##cname, __VA_ARGS__)

#define ALIGN_TO(x, a) ((x) + ((a) - 1)) & ~((a) - 1);

#ifdef __x86_64__
    #define GP_MAX 6
    #define SSE_MAX 8
#else
    #error "Unsupported architecture"
#endif

struct ffi_ccall {
    int gp_used;
    int sse_used;

    intptr_t gp[GP_MAX];
    intptr_t sse[SSE_MAX];

    uint8_t* stack_args;
    int stack_capacity;
    int stack_size;
};

struct ffi_type_ops {
    const char* name;
    struct shard_value (*read)(volatile struct shard_evaluator* e, const void* address, struct shard_set* ffi_type);
    void (*write)(volatile struct shard_evaluator* e, void* address, struct shard_value value, struct shard_set* ffi_type);
    void (*as_ccall_arg)(volatile struct shard_evaluator* e, struct ffi_ccall* ccall, struct shard_set* ffi_type, struct ffi_type_ops* ops, struct shard_value value);
    struct shard_value (*from_ccall_return)(volatile struct shard_evaluator* e, const intptr_t gp[2], const intptr_t sse[2], const uint8_t* buffer, struct shard_set* ffi_type, struct ffi_type_ops* ops);
};

FFI_BUILTIN("system.ffi.void.__toString", void_toString, SHARD_VAL_SET);
FFI_BUILTIN("system.ffi.cPointer", cPointer, SHARD_VAL_SET);
FFI_BUILTIN("system.ffi.cUnion", cUnion, SHARD_VAL_SET);
FFI_BUILTIN("system.ffi.cStruct", cStruct, SHARD_VAL_SET);
FFI_BUILTIN("system.ffi.cFunc", cFunc, SHARD_VAL_SET, SHARD_VAL_LIST);
FFI_BUILTIN("system.ffi.cCall", cCall, SHARD_VAL_SET);
FFI_BUILTIN("system.ffi.cValue", cValue, SHARD_VAL_SET);
FFI_BUILTIN("system.ffi.cAssign", cAssign, SHARD_VAL_SET, SHARD_VAL_ANY);

static struct shard_builtin* ffi_builtins[] = {
    &ffi_builtin_cPointer,
    &ffi_builtin_cFunc,
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

static void ccall_stack_arg(volatile struct shard_evaluator* e, struct ffi_ccall* ccall, struct shard_set* ffi_type, struct ffi_type_ops* ops, struct shard_value value) {
    int size = c_typesz(e, ffi_type);

    if(ccall->stack_size + size >= ccall->stack_capacity) {
        ccall->stack_capacity = MAX(ccall->stack_capacity * 2ul, sizeof(void*) * 64);
        while(ccall->stack_capacity < ccall->stack_size + size)
            ccall->stack_capacity *= 2;

        if(ccall->stack_size == 0)
            ccall->stack_args = shard_gc_malloc(e->gc, ccall->stack_capacity);
        else
            ccall->stack_args = shard_gc_realloc(e->gc, ccall->stack_args, ccall->stack_capacity);
    }

    ops->write(e, ccall->stack_args + ccall->stack_size, value, ffi_type);
    ccall->stack_size += size;
}

static void ccall_gp_arg(volatile struct shard_evaluator* e, struct ffi_ccall* ccall, struct shard_set* ffi_type, struct ffi_type_ops* ops, struct shard_value value) {
    if(ccall->gp_used >= (int) __len(ccall->gp)) {
        ccall_stack_arg(e, ccall, ffi_type, ops, value);
        return;
    }

    ops->write(e, ccall->gp + ccall->gp_used, value, ffi_type); 
    ccall->gp_used++;
}

static void ccall_sse_arg(volatile struct shard_evaluator* e, struct ffi_ccall* ccall, struct shard_set* ffi_type, struct ffi_type_ops* ops, struct shard_value value) {
    if(ccall->sse_used >= (int) __len(ccall->sse)) {
        ccall_stack_arg(e, ccall, ffi_type, ops, value);
        return;
    }

    ops->write(e, ccall->sse + ccall->sse_used, value, ffi_type); 
    ccall->sse_used++;
}

static void ccall_void_arg(volatile struct shard_evaluator* e, struct ffi_ccall*, struct shard_set*, struct ffi_type_ops*, struct shard_value) {
    shard_eval_throw(e, e->error_scope->loc, "cannot use ffi type `void` as a ccall argument");
}

static struct shard_value ccall_void_return(volatile struct shard_evaluator*, const intptr_t[2], const intptr_t[2], const uint8_t*, struct shard_set*, struct ffi_type_ops*) {
    return SET_VAL(ffi_void);
}

static struct shard_value ccall_gp_return(volatile struct shard_evaluator* e, const intptr_t gp[2], const intptr_t sse[2], const uint8_t* buffer, struct shard_set* ffi_type, struct ffi_type_ops* ops) {
    (void) sse;
    (void) buffer;
    return ops->read(e, gp, ffi_type);
}

static struct shard_value ccall_sse_return(volatile struct shard_evaluator* e, const intptr_t gp[2], const intptr_t sse[2], const uint8_t* buffer, struct shard_set* ffi_type, struct ffi_type_ops* ops) {
    (void) gp;
    (void) buffer;
    return ops->read(e, sse, ffi_type); 
}

struct ffi_type_ops ffi_pointer_ops = FFI_OPS(pointer, pointer, ccall_gp_arg, ccall_gp_return);
struct ffi_type_ops ffi_function_ops = FFI_OPS(function, pointer, ccall_gp_arg, ccall_gp_return);

static struct shard_value gen_cType(struct shard_context* ctx, const char* name, size_t size, size_t align, int extra) {
    struct shard_set* set = shard_set_init(ctx, 3 + extra); 

    shard_set_put(set, shard_get_ident(ctx, "name"), shard_unlazy(ctx, CSTRING_VAL(name)));
    shard_set_put(set, shard_get_ident(ctx, "size"), shard_unlazy(ctx, INT_VAL(size)));
    shard_set_put(set, shard_get_ident(ctx, "align"), shard_unlazy(ctx, INT_VAL(align)));

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

static struct shard_set* c_fields(volatile struct shard_evaluator* e, struct shard_set* ffi_type) {
    if(!c_typeis(e, ffi_type, "struct") && !c_typeis(e, ffi_type, "union"))
        return NULL;

    struct shard_lazy_value* lazy_fields;
    if(shard_set_get(ffi_type, shard_get_ident(e->ctx, "fields"), &lazy_fields))
        return NULL;

    struct shard_value fields_val = shard_eval_lazy2(e, lazy_fields);
    if(fields_val.type != SHARD_VAL_SET)
        return NULL;
    return fields_val.set;
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

static bool c_bigstruct(volatile struct shard_evaluator* e, struct shard_set* ffi_type) {
    int size = c_typesz(e, ffi_type);
    int align = c_typeal(e, ffi_type);
    if(size > 16 || align != 8)
        return false;

    struct shard_set* fields = c_fields(e, ffi_type);
    return fields->size == 2;
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

int ffi_load(struct shard_context* ctx) {
    int err;

    shard_hashmap_init(ctx, &ffi_type_ops, 32);

    shard_hashmap_put(ctx, &ffi_type_ops, "pointer", &ffi_pointer_ops);
    shard_hashmap_put(ctx, &ffi_type_ops, "function", &ffi_function_ops);

    ffi_void = shard_set_init(ctx, 1);
    shard_set_put(ffi_void, shard_get_ident(ctx, "__toString"), shard_unlazy(ctx, BUILTIN_VAL(&ffi_builtin_void_toString)));

    struct {
        const char* ident;
        struct shard_value value;
        struct ffi_type_ops ops;
    } ffi_constants[] = {
        {"system.ffi.void", SET_VAL(ffi_void), {NULL, NULL, NULL, NULL, NULL}},

        {"system.ffi.cVoid", PRIMITIVE_CTYPE(ctx, void), FFI_OPS(void, void, ccall_void_arg, ccall_void_return)},
        {"system.ffi.cBool", PRIMITIVE_CTYPE(ctx, bool), FFI_OPS(bool, bool, ccall_gp_arg, ccall_gp_return)},

        {"system.ffi.cChar", PRIMITIVE_CTYPE(ctx, char), FFI_OPS(char, char, ccall_gp_arg, ccall_gp_return)},
        {"system.ffi.cShort", PRIMITIVE_CTYPE(ctx, short), FFI_OPS(short, short, ccall_gp_arg, ccall_gp_return)},
        {"system.ffi.cInt", PRIMITIVE_CTYPE(ctx, int), FFI_OPS(int, int, ccall_gp_arg, ccall_gp_return)},
        {"system.ffi.cLong", PRIMITIVE_CTYPE(ctx, long), FFI_OPS(long, long, ccall_gp_arg, ccall_gp_return)},
        {"system.ffi.cLongLong", PRIMITIVE_CTYPE(ctx, long long), FFI_OPS(long long, longlong, ccall_gp_arg, ccall_gp_return)},

        {"system.ffi.cUnsignedChar", PRIMITIVE_CTYPE(ctx, unsigned char), FFI_OPS(unsigned char, char, ccall_gp_arg, ccall_gp_return)},
        {"system.ffi.cUnsignedShort", PRIMITIVE_CTYPE(ctx, unsigned short), FFI_OPS(unsigned short, short, ccall_gp_arg, ccall_gp_return)},
        {"system.ffi.cUnsignedInt", PRIMITIVE_CTYPE(ctx, unsigned int), FFI_OPS(unsigned int, int, ccall_gp_arg, ccall_gp_return)},
        {"system.ffi.cUnsignedLong", PRIMITIVE_CTYPE(ctx, unsigned long), FFI_OPS(unsigned long, long, ccall_gp_arg, ccall_gp_return)},
        {"system.ffi.cUnsignedLongLong", PRIMITIVE_CTYPE(ctx, unsigned long long), FFI_OPS(unsingled long long, longlong, ccall_gp_arg, ccall_gp_return)},

        {"system.ffi.cFloat", PRIMITIVE_CTYPE(ctx, float), FFI_OPS(float, float, ccall_sse_arg, ccall_sse_return)},
        {"system.ffi.cDouble", PRIMITIVE_CTYPE(ctx, double), FFI_OPS(double, double, ccall_sse_arg, ccall_sse_return)},

        {"system.ffi.cSizeT", PRIMITIVE_CTYPE(ctx, size_t), FFI_OPS(size_t, size_t, ccall_gp_arg, ccall_gp_return)},
    };

    for(size_t i = 0; i < __len(ffi_constants); i++) {
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

struct shard_value ffi_bind(volatile struct shard_evaluator* e, const char* symbol_name, void* symbol_address, struct shard_set* ffi_type) {
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

void ffi_shard_to_c(volatile struct shard_evaluator* e, struct shard_value value, void* symbol_address, struct shard_set* ffi_type) {
    const char* name = c_typename(e, ffi_type);
    assert(name != NULL);

    struct ffi_type_ops* ops = shard_hashmap_get(&ffi_type_ops, name);
    if(!ops)
        shard_eval_throw(e, e->error_scope->loc, "unexpected ffi typename `%s`", name);

    ops->write(e, symbol_address, value, ffi_type);
}

static inline void ffi_pusharg(volatile struct shard_evaluator* e, struct ffi_ccall* ccall, struct shard_value value, struct shard_set* ffi_type) {
    const char* name = c_typename(e, ffi_type);
    assert(name != NULL);

    struct ffi_type_ops* ops = shard_hashmap_get(&ffi_type_ops, name);
    if(!ops)
        shard_eval_throw(e, e->error_scope->loc, "unexpected ffi typename `%s`", name);

    ops->as_ccall_arg(e, ccall, ffi_type, ops, value);
}

static inline struct shard_value ffi_fromret(volatile struct shard_evaluator* e, const intptr_t gp[2], const intptr_t sse[2], const uint8_t* buffer, struct shard_set* ffi_type) {
    const char* name = c_typename(e, ffi_type);
    assert(name != NULL);

    struct ffi_type_ops* ops = shard_hashmap_get(&ffi_type_ops, name);
    if(!ops)
        shard_eval_throw(e, e->error_scope->loc, "unexpected ffi typename `%s`", name);

    return ops->from_ccall_return(e, gp, sse, buffer, ffi_type, ops);
}

static void ffi_doccall(struct ffi_ccall* ccall, void* address, intptr_t gp_result[2], intptr_t sse_result[2]) {
#ifdef __x86_64__

    __asm__ volatile (_("\
            // ffi_docall\n\
            movq %[calladdr], %%r10\n\
            test %[stacksz], %[stacksz]\n\
            jz .L.no_stack\n\
            mov %[stackbuf], %%rsi\n\
            mov %%rsp, %%rdi\n\
            sub %[stacksz], %%rdi\n\
            mov %[stacksz], %%rcx\n\
            mov $0, %%al\n\
            rep movsb\n\
        .L.no_stack:\n\
            test %[sse_used], %[sse_used]\n\
            jz .L.no_sse\n\
            movq 0(%[sse]), %%xmm0\n\
            movq 8(%[sse]), %%xmm1\n\
            movq 16(%[sse]), %%xmm2\n\
            movq 24(%[sse]), %%xmm3\n\
            movq 32(%[sse]), %%xmm4\n\
            movq 40(%[sse]), %%xmm5\n\
            movq 48(%[sse]), %%xmm6\n\
            movq 56(%[sse]), %%xmm7\n\
        .L.no_sse:\n\
            test %[gp_used], %[gp_used]\n\
            jz .L.no_gp\n\
            movq 0(%[gp]), %%rdi  \n\
            movq 8(%[gp]), %%rsi  \n\
            movq 16(%[gp]), %%rcx \n\
            movq 24(%[gp]), %%rdx \n\
            movq 32(%[gp]), %%r8  \n\
            movq 40(%[gp]), %%r9  \n\
        .L.no_gp:\n\
            sub %[stacksz], %%rsp\n\
            call *%%r10\n\
            add %[stacksz], %%rsp\n\
            movq %%rax, %[gpr0]\n\
            movq %%rdx, %[gpr1]\n\
            movq %%xmm0, %[sser0]\n\
            movq %%xmm1, %[sser1]\n\
        ")
        :   [gpr0] "=m"(gp_result[0]),
            [gpr1] "=m"(gp_result[1]), 
            [sser0] "=m"(sse_result[0]), 
            [sser1] "=m"(sse_result[1])
        :   [gp] "r"(ccall->gp), [sse] "r"(ccall->sse), 
            [stacksz]   "r"((size_t) ccall->stack_size),
            [gp_used]   "r"(ccall->gp_used),
            [sse_used]  "r"(ccall->sse_used),
            [stackbuf]  "m"(ccall->stack_args),
            [calladdr]  "m"(address),
            [gpr]       "m"(gp_result),
            [sser]      "m"(sse_result)
        :   "rax", "rsi", "rdi", "rcx", "rdx", "r8", "r9", "r10", "r11",
            "xmm0", "xmm1", "xmm2", "xmm3", "xmm4", "xmm5", "xmm6", "xmm7"
    );

#else
    #error "Unsupported architecture"
#endif
}

static struct shard_value builtin_void_toString(volatile struct shard_evaluator*, struct shard_builtin*, struct shard_lazy_value**) {
    return CSTRING_VAL("void");
}

static struct shard_value builtin_cPointer(volatile struct shard_evaluator* e, struct shard_builtin* builtin, struct shard_lazy_value** args) {
    struct shard_value type = gen_cType(e->ctx, "pointer", sizeof(void*), _Alignof(void*), 1); 

    shard_builtin_eval_arg(e, builtin, args, 0);
    shard_set_put(type.set, shard_get_ident(e->ctx, "base"), args[0]);

    return type;
}

static struct shard_value builtin_cFunc(volatile struct shard_evaluator* e, struct shard_builtin* builtin, struct shard_lazy_value** args) {
    struct shard_value type = gen_cType(e->ctx, "function", sizeof(void (*)()), _Alignof(void (*)()), 2);

    shard_builtin_eval_arg(e, builtin, args, 0);
    shard_set_put(type.set, shard_get_ident(e->ctx, "returns"), args[0]);

    shard_builtin_eval_arg(e, builtin, args, 1);
    shard_set_put(type.set, shard_get_ident(e->ctx, "arguments"), args[1]);

    return type;
}

static struct shard_value builtin_cStruct(volatile struct shard_evaluator* e, struct shard_builtin* builtin, struct shard_lazy_value** args) {
    struct shard_value fields = shard_builtin_eval_arg(e, builtin, args, 0);
    int size = 0;
    int align = 0;

    for(size_t i = 0; i < fields.set->capacity; i++) {
        if(!fields.set->entries[i].key)
            continue;

        struct shard_value field_val = shard_eval_lazy2(e, fields.set->entries[i].value);
        if(field_val.type != SHARD_VAL_SET)
            shard_eval_throw(e, e->error_scope->loc, "field `%s` passed to `system.ffi.cStruct` is not a ffi type", fields.set->entries[i].key);    
        
        int field_size = c_typesz(e, field_val.set);
        if(field_size < 0)
            shard_eval_throw(e, e->error_scope->loc, "field `%s` passed to `system.ffi.cStruct` is missing `size` attr", fields.set->entries[i].key);

        int field_align = c_typeal(e, field_val.set);
        if(field_align < 0)
            shard_eval_throw(e, e->error_scope->loc, "field `%s` passed to `system.ffi.cStruct` is missing `align` attr", fields.set->entries[i].key);

        assert(!"unimplemented");
    }

    struct shard_value type = gen_cType(e->ctx, "struct", size, align, 1); 
    shard_set_put(type.set, shard_get_ident(e->ctx, "fields"), args[0]);

    return type;
}

static struct shard_value builtin_cUnion(volatile struct shard_evaluator* e, struct shard_builtin* builtin, struct shard_lazy_value** args) {
    struct shard_value fields = shard_builtin_eval_arg(e, builtin, args, 0);
    int size = 0;
    int align = 0;

    for(size_t i = 0; i < fields.set->capacity; i++) {
        if(!fields.set->entries[i].key)
            continue;

        struct shard_value field_val = shard_eval_lazy2(e, fields.set->entries[i].value);
        if(field_val.type != SHARD_VAL_SET)
            shard_eval_throw(e, e->error_scope->loc, "field `%s` passed to `system.ffi.cUnion` is not a ffi type", fields.set->entries[i].key);    
        
        int field_size = c_typesz(e, field_val.set);
        if(field_size < 0)
            shard_eval_throw(e, e->error_scope->loc, "field `%s` passed to `system.ffi.cUnion` is missing `size` attr", fields.set->entries[i].key);

        int field_align = c_typeal(e, field_val.set);
        if(field_align < 0)
            shard_eval_throw(e, e->error_scope->loc, "field `%s` passed to `system.ffi.cUnion` is missing `align` attr", fields.set->entries[i].key);
        
        align = MAX(field_align, align);
        size = MAX(field_size, size);
    }

    struct shard_value type = gen_cType(e->ctx, "union", size, align, 1); 
    shard_set_put(type.set, shard_get_ident(e->ctx, "fields"), args[0]);

    return type;
}

static struct shard_value builtin_cCall(volatile struct shard_evaluator* e, struct shard_builtin* builtin, struct shard_lazy_value** args) {
    struct shard_value binding = shard_builtin_eval_arg(e, builtin, args, 0);

    void* address = binding_address(e, binding.set);
    if(!address)
        shard_eval_throw(e, e->error_scope->loc, "invalid or missing attr `address` of binding passed to `system.ffi.cCall`");

    struct shard_set* ffi_type = binding_ffi_type(e, binding.set);
    if(!ffi_type)
        shard_eval_throw(e, e->error_scope->loc, "invalid or missing attr `type` of binding passed to `system.ffi.cCall`");

    if(!c_typeis(e, ffi_type, "function"))
        shard_eval_throw(e, e->error_scope->loc, "`system.ffi.cCall` can only call ffi bindings of type `function`");

    struct shard_list* arg_type_list = c_arglist(e, ffi_type);

    struct shard_set* return_type = c_return(e, ffi_type);
    if(!return_type)
        shard_eval_throw(e, e->error_scope->loc, "could not get return type from ffi binding passed to `system.ffi.cCall`");

    size_t arg_count = shard_list_length(arg_type_list);

    struct ffi_ccall ccall;
    memset(&ccall, 0, sizeof(struct ffi_ccall));

    uint8_t* return_buffer = NULL;
    if(c_bigstruct(e, return_type)) {
        return_buffer = alloca(c_typesz(e, return_type));
        ccall.gp[ccall.gp_used++] = (intptr_t) return_buffer;
    }
    
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

        ffi_pusharg(e, &ccall, arg_val, arg_type.set);
    }

    intptr_t gp_result[2], sse_result[2];
    ffi_doccall(&ccall, address, gp_result, sse_result);

    if(ccall.stack_size > 0)
        shard_gc_free(e->gc, ccall.stack_args);

    return ffi_fromret(e, gp_result, sse_result, return_buffer, return_type);
}

static struct shard_value builtin_cValue(volatile struct shard_evaluator* e, struct shard_builtin* builtin, struct shard_lazy_value** args) {
    struct shard_value binding = shard_builtin_eval_arg(e, builtin, args, 0);
   
    void* address = binding_address(e, binding.set);
    if(!address)
        shard_eval_throw(e, e->error_scope->loc, "invalid or missing attr `address` of binding passed to `system.ffi.cValue`");
   
    struct shard_set* ffi_type = binding_ffi_type(e, binding.set);
    if(!ffi_type)
        shard_eval_throw(e, e->error_scope->loc, "invalid or missing attr `type` of binding passed to `system.ffi.cValue`");
    
    return ffi_c_to_shard(e, address, ffi_type);
}

static struct shard_value builtin_cAssign(volatile struct shard_evaluator* e, struct shard_builtin* builtin, struct shard_lazy_value** args) {
    struct shard_value binding = shard_builtin_eval_arg(e, builtin, args, 0);
    struct shard_value value = shard_builtin_eval_arg(e, builtin, args, 1);

    void* address = binding_address(e, binding.set);
    if(!address)
        shard_eval_throw(e, e->error_scope->loc, "invalid or missing attr `address` of binding passed to `system.ffi.cAssign`");

    struct shard_set* ffi_type = binding_ffi_type(e, binding.set);
    if(!ffi_type)
        shard_eval_throw(e, e->error_scope->loc, "invalid or missing attr `type` of binding passed to `system.ffi.cAssign`");

    ffi_shard_to_c(e, value, address, ffi_type);
    return value;
}

