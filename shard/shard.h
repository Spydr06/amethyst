#ifndef _SHARD_H
#define _SHARD_H

#include <libshard.h>

#define EITHER(a, b) ((a) ? (a) : (b))
#define MAX(a, b) ((a) > (b) ? (a) : (b))
#define LEN(arr) (sizeof((arr)) / sizeof((arr)[0]))

#define C_RED "\033[31m"
#define C_GREEN "\033[32m"
#define C_YELLOW "\033[93m"
#define C_BLACK "\033[90m"
#define C_BLUE "\033[34m"
#define C_PURPLE "\033[35m"

#define C_BLD "\033[1m"
#define C_RST "\033[0m"
#define C_NOBLD "\033[22m"

#if defined(_WIN32) || defined(_WIN64)
    #define PLATFORM_OS "windows"
#elif defined(__linux__)
    #define PLATFORM_OS "linux"
#elif defined(__APPLE__) && defined(__MACH__)
    #define PLATFORM_OS "macos"
#elif defined(__unix__)
    #define PLATFORM_OS "unix"
#else
    #define PLATFORM_OS "unknown"
#endif

#if defined(__x86_64__) || defined(_M_X64)
    #define PLATFORM_ARCH "x86_64"
#elif defined(__i386) || defined(_M_IX86)
    #define PLATFORM_ARCH "x86"
#elif defined(__arm__) || defined(_M_ARM)
    #define PLATFORM_ARCH "arm"
#elif defined(__aarch64__)
    #define PLATFORM_ARCH "aarch64"
#else
    #define PLATFORM_ARCH "unknown"
#endif

#define PLATFORM_STRING PLATFORM_OS "-" PLATFORM_ARCH

#define REPL_ORIGIN "repl"

#ifndef __len
    #define __len(arr) (sizeof((arr)) / sizeof(*(arr)))
#endif

#define _(str) str

void print_file_error(struct shard_error* error);

int shard_repl(const char* progname, struct shard_context* ctx, bool echo_result);

int load_ext_builtins(struct shard_context* ctx, int argc, char** argv);

int ffi_load(struct shard_context* ctx);

struct shard_value ffi_bind(volatile struct shard_evaluator* e, const char* symbol_name, void* symbol_address, struct shard_set* ffi_type);

struct jit_buffer {
    uint8_t* buffer;
    size_t capacity;
    size_t fill;
};

extern const uint8_t jit_prologue[];
extern const size_t jit_prologue_size;

extern const uint8_t jit_epilogue[];
extern const size_t jit_epilogue_size;

int jit_buffer_create(struct jit_buffer* buffer);
int jit_buffer_delete(struct jit_buffer* buffer);

int jit_append(struct jit_buffer* buffer, const uint8_t* code, size_t size);
uintptr_t jit_jump(struct jit_buffer* buffer, size_t offset);

#endif /* _SHARD_H  */
