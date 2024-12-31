#include "stdlib.h"
#include <stdint.h>

#include <sys/syscall.h>
#include <internal/syscall.h>

struct exec_stack_data {
    int argc;
    char** argv;
    char** envp;
};

static void parse_exec_stack(uintptr_t* stack_base, struct exec_stack_data* stack_data) {
    stack_data->argc = *stack_base++;
    stack_data->argv = (char**) stack_base;
    stack_base += stack_data->argc;
    stack_base++; // skip NULL element
    stack_data->envp = (char**) stack_base;
}

extern _Noreturn void __libc_entry(int (*main_fn)(int argc, char* argv[], char* env[]), uintptr_t* stack_base) {
    struct exec_stack_data stack_data;
    parse_exec_stack(stack_base, &stack_data);

    int exit_code = main_fn(stack_data.argc, stack_data.argv, stack_data.envp);

    _Exit(exit_code);
}

