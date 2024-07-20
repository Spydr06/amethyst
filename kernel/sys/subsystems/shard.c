#include "kernelio.h"
#include <sys/subsystems/shard.h>
#include <sys/spinlock.h>

#include <mem/heap.h>

#include <libshard.h>
#include <libshard-util.h>

static spinlock_t context_lock;
static struct shard_context context;

#define CURRENT_SYSTEM "amethyst-x86_64"

static void print_error(struct shard_error* error) {
    klog(ERROR, "%s:%d:%d: %s", error->loc.src->origin, error->loc.line, error->loc.offset, error->err);
}

void shard_subsystem_init(void) {
    spinlock_init(context_lock);
    spinlock_acquire(&context_lock);

    context = (struct shard_context) {
        .malloc = kmalloc,
        .realloc = krealloc,
        .free = kfree,
        .realpath = NULL,
        .dirname = NULL,
        .access = NULL,
        .open = NULL,
        .home_dir = NULL
    };

    int err = shard_init(&context);
    if(err)
        panic("error initializing shard subsystem: %s\n", strerror(err)); 

    shard_set_current_system(&context, CURRENT_SYSTEM);

    spinlock_release(&context_lock);

    klog(INFO, "shard subsystem intialized");
}

