#include "initsys.h"

#include <libshard.h>
#include <shard_libc_driver.h>

#include <sys/module.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>

static int load_module(const char *path) {
    int fd = open(path, O_RDONLY, 0);
    if(fd < 0)
        return errno;

    if(finit_module(fd, (const char* const[]){NULL}, 0))
        return errno;

    close(fd);
    return 0;
}

int load_kernel_modules(struct shard_context *ctx, const char *path) {
    struct shard_open_source *source = shard_open(ctx, path);
    if(!source) {
        fprintf(stderr, "Could not open module list `%s`: %s\n", path, strerror(errno));
        return errno;
    }
    
    int num_errors = shard_eval(ctx, source);
    if(num_errors) {
        shard_emit_errors(ctx);
        return EINVAL;
    }

    struct shard_value *module_list = &source->result;
    if(!(module_list->type & SHARD_VAL_LIST)) {
        fprintf(stderr, "Module list `%s` does not evaluate to list.\n", path);
        return EINVAL;
    }

    struct shard_list *head = module_list->list.head;
    for(size_t i = 0; head; i++, head = head->next) {
        if((num_errors = shard_eval_lazy(ctx, head->value))) {
            shard_emit_errors(ctx);
            return EINVAL;
        }

        struct shard_value *module_path = &head->value->eval;
        if(!(module_path->type & SHARD_VAL_TEXTUAL)) {
            fprintf(stderr, "Module list entry [%zu] does not evaluate to string or path.\n", i);
            return EINVAL;
        }

        int err = load_module(module_path->string);
        if(err) {
            fprintf(stderr, "Could not load kernel module `%s`: %s\n", module_path->string, strerror(err));
            return err;
        }
    }

    return 0;
}

