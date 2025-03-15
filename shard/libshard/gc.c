#define _LIBSHARD_INTERNAL
#include <libshard.h>
#include <libshard-internal.h>

#ifndef SHARD_USE_GCBOEHM

const enum shard_gc_backend _shard_gc_backend = SHARD_GC_BUILTIN;

#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <setjmp.h>

struct shard_gc {
    struct shard_context* ctx;
    struct shard_alloc_map* allocs;
    void* stack_bottom;
    size_t min_size;
    bool paused;
};

// TODO: check if allocations succeed!

// The garbage collector is largely inspired by https://github.com/mkirchner/gc
// Check out mkirchner's repo and license for more information!

// temporary allocation markings
#define GC_TAG_NONE 0
#define GC_TAG_ROOT 1 /* not automatically collected */
#define GC_TAG_MARK 2 /* part of mark-sweep */

struct gc_allocation {
    void* ptr;                  // mem ptr
    size_t size;                // allocated size
    uint8_t tag;                // tag for mark-and-sweep
    void (*dtor)(void*);        // destructor
    struct gc_allocation* next;
};

static struct gc_allocation* gc_allocation_new(struct shard_context* ctx, void* ptr, size_t size, void (*dtor)(void*)) {
    struct gc_allocation* a = (struct gc_allocation*) ctx->malloc(sizeof(struct gc_allocation));
    a->ptr = ptr;
    a->size = size;
    a->tag = GC_TAG_NONE;
    a->dtor = dtor;
    a->next = NULL;
    return a;
}

static void gc_allocation_delete(struct shard_context* ctx, struct gc_allocation* a) {
    ctx->free(a);
}

// hashmap that holds the allocation objects
struct shard_alloc_map {
    size_t capacity;
    size_t min_capacity;
    double downsize_factor;
    double upsize_factor;
    double sweep_factor;
    size_t sweep_limit;
    size_t size;
    struct gc_allocation** allocs;
};

inline static double gc_allocation_map_load_factor(struct shard_alloc_map* map) {
    return (double) map->size / (double) map->capacity;
}

inline static bool is_prime(size_t n) {
    /* https://stackoverflow.com/questions/1538644/c-determine-if-a-number-is-prime */
    if (n <= 3)
        return n > 1;     // as 2 and 3 are prime
    else if (n % 2 == 0 || n % 3 == 0)
        return false;     // check if n is divisible by 2 or 3
    else {
        for (size_t i = 5; i * i <= n; i += 6) {
            if (n % i == 0 || n % (i + 2) == 0)
                return false;
        }
        return true;
    }
}

inline static size_t next_prime(size_t n) {
    while(!is_prime(n)) ++n;
    return n;
}

static struct shard_alloc_map* gc_allocation_map_new(struct shard_context* ctx, size_t min_capacity, size_t capacity, double sweep_factor, double downsize_factor, double upsize_factor) {
    struct shard_alloc_map* map = (struct shard_alloc_map*) ctx->malloc(sizeof(struct shard_alloc_map));
    map->min_capacity = next_prime(min_capacity);
    map->capacity = next_prime(capacity);
    if(map->capacity < map->min_capacity)
        map->capacity = map->min_capacity;
    map->sweep_factor = sweep_factor;
    map->sweep_limit = (int) (sweep_factor * map->capacity);
    map->downsize_factor = downsize_factor;
    map->upsize_factor = upsize_factor;
    map->allocs = (struct gc_allocation**) ctx->malloc(map->capacity * sizeof(struct gc_allocation*));
    memset(map->allocs, 0, map->capacity * sizeof(struct gc_allocation*));
    map->size = 0;

    DBG_PRINTF("created allocation map (cap=%ld, size=%ld)\n", map->capacity, map->size);
    
    return map;
}

static void gc_allocation_map_delete(struct shard_context* ctx, struct shard_alloc_map* map) {
    DBG_PRINTF("deleting allocation map (cap=%ld, size=%ld)\n", map->capacity, map->size);

    struct gc_allocation *alloc, *tmp;
    for(size_t i = 0; i < map->capacity; i++)
        if((alloc = map->allocs[i]))
            while(alloc) {
                tmp = alloc;
                alloc = alloc->next;
                gc_allocation_delete(ctx, tmp);
            }

    ctx->free(map->allocs);
    ctx->free(map);
}

static inline size_t gc_hash(void* ptr) {
    return ((uintptr_t) ptr) >> 3;
}

static void gc_allocation_map_resize(struct shard_context* ctx, struct shard_alloc_map* map, size_t new_capacity) {
    if(new_capacity < map->min_capacity)
        return;

    DBG_PRINTF("resizing allocation map (cap=%ld, size=%ld) -> (cap=%ld)\n", map->capacity, map->size, new_capacity);

    struct gc_allocation** resized_allocs = ctx->malloc(new_capacity * sizeof(struct gc_allocation*));
    memset(resized_allocs, 0, sizeof(struct gc_allocation*));

    for(size_t i = 0; i < map->capacity; i++) {
        struct gc_allocation* alloc = map->allocs[i];
        while(alloc) {
            struct gc_allocation* next_alloc = alloc->next;
            size_t new_index = gc_hash(alloc->ptr) % new_capacity;
            alloc->next = resized_allocs[new_index];
            resized_allocs[new_index] = alloc;
            alloc = next_alloc;
        }
    }

    ctx->free(map->allocs);
    map->capacity = new_capacity;
    map->allocs = resized_allocs;
    map->sweep_limit = map->size + map->sweep_factor * (map->capacity - map->size);
}

static bool gc_allocation_map_resize_to_fit(struct shard_context* ctx, struct shard_alloc_map* map) {
    double load_factor = gc_allocation_map_load_factor(map);
    if(load_factor > map->upsize_factor) {
        DBG_PRINTF("load factor %0.3g > %0.3g !! upsizing\n", load_factor, map->upsize_factor);
        gc_allocation_map_resize(ctx, map, next_prime(map->capacity * 2));
        return true;
    }
    if(load_factor < map->downsize_factor) {
        DBG_PRINTF("load factor %0.3g < %0.3g !! downsizing\n", load_factor, map->downsize_factor);
        gc_allocation_map_resize(ctx, map, next_prime(map->capacity / 2));
        return true;
    }
    return false;
}

static struct gc_allocation* gc_allocation_map_get(struct shard_alloc_map* map, void* ptr) {
    size_t index = gc_hash(ptr) % map->capacity;
    struct gc_allocation* cur = map->allocs[index];
    while(cur) {
        if(cur->ptr == ptr) {
            return cur;
        }
        cur = cur->next;
    }
    return NULL;
}

static struct gc_allocation* gc_allocation_map_put(struct shard_context* ctx, struct shard_alloc_map* map, void* ptr, size_t size, void (*dtor)(void*)) {
    size_t index = gc_hash(ptr) % map->capacity;

    DBG_PRINTF("put request for allocation idx=%ld\n", index);

    struct gc_allocation* alloc = gc_allocation_new(ctx, ptr, size, dtor);
    struct gc_allocation* cur = map->allocs[index];
    struct gc_allocation* prev = NULL;

    while(cur != NULL) {
        if(cur->ptr == ptr) {
            // found it
            alloc->next = cur->next;
            if(!prev) // position 0
                map->allocs[index] = alloc;
            else
                prev->next = alloc;
            gc_allocation_delete(ctx, cur);

            DBG_PRINTF("allocation map upsert at idx=%ld\n", index);
            return alloc;
        }

        prev = cur;
        cur = cur->next;
    }

    // insert at the front
    cur = map->allocs[index];
    alloc->next = cur;
    map->allocs[index] = alloc;
    map->size++;

    DBG_PRINTF("allocation map insert at idx=%ld\n", index);

    void* p = alloc->ptr;
    if(gc_allocation_map_resize_to_fit(ctx, map))
        alloc = gc_allocation_map_get(map, p);
    return alloc;
}

static void gc_allocation_map_remove(struct shard_context* ctx, struct shard_alloc_map* map, void* ptr, bool allow_resize) {
    size_t index = gc_hash(ptr) % map->capacity;
    struct gc_allocation* cur = map->allocs[index];
    struct gc_allocation* prev = NULL;
    struct gc_allocation* next;

    while(cur != NULL) {
        next = cur->next;
        if(cur->ptr == ptr) {
            // found it
            if(!prev)
                map->allocs[index] = cur->next;
            else
                prev->next = cur->next;
            gc_allocation_delete(ctx, cur);
            map->size--;
        }
        else // move on
            prev = cur;
        cur = next;
    }

    if(allow_resize)
        gc_allocation_map_resize_to_fit(ctx, map);
}

static inline void* gc_mcalloc(volatile struct shard_gc* gc, size_t count, size_t size) {
    if(!count)
        return gc->ctx->malloc(size);
    void* ptr = gc->ctx->malloc(count * size);
    if(ptr)
        memset(ptr, 0, count * size);
    return ptr;
}

static inline bool gc_needs_sweep(volatile struct shard_gc* gc) {
    return gc->allocs->size > gc->allocs->sweep_limit;
}

static void* allocate(volatile struct shard_gc* gc, size_t count, size_t size, void (*dtor)(void*)) {
    if(gc_needs_sweep(gc) && !gc->paused) {
#ifdef RUNTIME_DEBUG
        size_t freed_mem = gc_run(gc);
        DBG_PRINTF("garbage collection cleaned up %lu bytes.", freed_mem);
#else
        shard_gc_run(gc);
#endif
    }

    void* ptr = gc_mcalloc(gc, size, count);
    size_t alloc_size = count ? count * size : size;

    // if allocation fails, collect garbage and try again
    if(!ptr && !gc->paused && (errno == EAGAIN || errno == ENOMEM)) {
        shard_gc_run(gc);
        ptr = gc_mcalloc(gc, count, size);
        if(!ptr)
            return NULL;
    }
    
    DBG_PRINTF("allocated %zu bytes at %p\n", alloc_size, (void*) ptr);

    struct gc_allocation* alloc = gc_allocation_map_put(gc->ctx, gc->allocs, ptr, alloc_size, dtor);

    if(alloc) {
        DBG_PRINTF("managing %zu bytes at %p\n", alloc_size, (void*) alloc->ptr);
        return alloc->ptr;
    }
    else {
        gc->ctx->free(ptr);
        return NULL;
    }
}

static void make_root(volatile struct shard_gc* gc, void* ptr)
{
    struct gc_allocation* alloc = gc_allocation_map_get(gc->allocs, ptr);
    if(alloc)
        alloc->tag |= GC_TAG_ROOT;
}

void shard_gc_begin_ext(
        volatile struct shard_gc* gc,
        struct shard_context* ctx,
        void* stack_bottom,
        size_t initial_capacity, size_t min_capacity, 
        double downsize_load_factor, double upsize_load_factor, double sweep_factor
    ) {
    double downsize_limit = downsize_load_factor > 0.0 ? downsize_load_factor : 0.2;
    double upsize_limit = upsize_load_factor > 0.0 ? upsize_load_factor : 0.8;
    sweep_factor = sweep_factor > 0.0 ? sweep_factor : 0.5;
    gc->ctx = ctx;
    gc->paused = false;
    gc->stack_bottom = stack_bottom;
    initial_capacity = initial_capacity < min_capacity ? min_capacity : initial_capacity;
    gc->allocs = gc_allocation_map_new(gc->ctx, min_capacity, initial_capacity, sweep_factor, downsize_limit, upsize_limit);

    DBG_PRINTF("created new garbage collector (cap=%ld, size=%ld)\n", gc->allocs->capacity, gc->allocs->size);
}

struct shard_gc* shard_gc_begin(struct shard_context* ctx, void* stack_bottom) {
    struct shard_gc* gc = ctx->malloc(sizeof(struct shard_gc));
    shard_gc_begin_ext(gc, ctx, stack_bottom, 1024, 1024, 0.2, 0.8, 0.7);
    return gc;
}

void shard_gc_pause(volatile struct shard_gc* gc) {
    gc->paused = true;
}

void shard_gc_resume(volatile struct shard_gc* gc) {
    gc->paused = false;
}

void* shard_gc_malloc_ext(volatile struct shard_gc* gc, size_t size, void (*dtor)(void*)) {
    return allocate(gc, 1, size, dtor);
}

void* shard_gc_calloc_ext(volatile struct shard_gc* gc, size_t nmemb, size_t size, void (*dtor)(void*)) {
    return allocate(gc, nmemb, size, dtor);
}

void* shard_gc_malloc(volatile struct shard_gc* gc, size_t size) {
    return allocate(gc, 1, size, NULL);
}

void* shard_gc_calloc(volatile struct shard_gc* gc, size_t nmemb, size_t size) {
    return allocate(gc, nmemb, size, NULL);
}

void* shard_gc_realloc(volatile struct shard_gc* gc, void* p, size_t size) {
    struct gc_allocation* alloc = gc_allocation_map_get(gc->allocs, p);
    if(p && !alloc) {
        // the user passed an unknown pointer
        errno = EINVAL;
        return NULL;
    }
    void* q = gc->ctx->realloc(p, size);
    if(!q) {
        // realloc failed but p is still valid
        return NULL;
    }
    if(!p) {
        // allocation, not reallocation
        struct gc_allocation* alloc = gc_allocation_map_put(gc->ctx, gc->allocs, q, size, NULL);
        return alloc->ptr;
    }
    if(p == q) {
        // successful reallocation w/o copy
        alloc->size = size;
    } else {
        // successful reallocation w/ copy
        void (*dtor)(void*) = alloc->dtor;
        gc_allocation_map_remove(gc->ctx, gc->allocs, p, true);
        gc_allocation_map_put(gc->ctx, gc->allocs, q, size, dtor);
    }
    return q;
}

void* shard_gc_make_static(volatile struct shard_gc* gc, void* ptr) {
    make_root(gc, ptr);
    return ptr;
}

void shard_gc_free(volatile struct shard_gc* gc, void* ptr)
{
    struct gc_allocation* alloc = gc_allocation_map_get(gc->allocs, ptr);
    if(!alloc) {
        DBG_PRINTF("ignoring request to free unknown ptr %p\n", ptr);
        return;
    }

    gc_allocation_map_remove(gc->ctx, gc->allocs, ptr, true);
    if(alloc->dtor)
        alloc->dtor(ptr);
    gc->ctx->free(ptr);
}

static void mark_alloc(volatile struct shard_gc* gc, void* ptr) {
    struct gc_allocation* alloc = gc_allocation_map_get(gc->allocs, ptr);
    if(!alloc || alloc->tag & GC_TAG_MARK)
        return;

    DBG_PRINTF("marking allocation (ptr=%p)\n", ptr);
    alloc->tag |= GC_TAG_MARK;

    // iterate over allocation contents
    DBG_PRINTF("checking allocation (ptr=%p, size=%lu) contents\n", ptr, alloc->size);
    for(uint8_t* p = (uint8_t*) alloc->ptr; p <= (uint8_t*) alloc->ptr + alloc->size - sizeof(uint8_t*); p++) {
        DBG_PRINTF("checking allocation (ptr=%p) @%lu with value %p\n", ptr, p - ((uint8_t*) alloc->ptr), *(void**)p);
        mark_alloc(gc, *(void**) p);
    }
}

static void mark_stack(volatile struct shard_gc* gc) {
    DBG_PRINTF("marking the stack (gc@%p) in increments of %ld\n", (void*) gc, sizeof(uint8_t));
    void* stack_top = __builtin_frame_address(0);
    void* stack_bottom = gc->stack_bottom;

    // stack grows from top to bottom
    for(uint8_t* p = (uint8_t*) stack_top; p <= (uint8_t*) stack_bottom - sizeof(uint8_t*); p++) {
        mark_alloc(gc, *(void**) p);
    }
}

static void mark_roots(volatile struct shard_gc* gc) {
    DBG_PRINTF("marking roots\n");
    for(size_t i = 0; i < gc->allocs->capacity; i++) {
        struct gc_allocation* chunk = gc->allocs->allocs[i];
        while(chunk) {
            if(chunk->tag & GC_TAG_ROOT) {
                DBG_PRINTF("marking root @ %p\n", chunk->ptr);
                mark_alloc(gc, chunk->ptr);
            }
            chunk = chunk->next;
        }
    }
}

static void mark(volatile struct shard_gc* gc) {
    DBG_PRINTF("initiating GC mark (gc@%p)\n", (void*) gc);
    mark_roots(gc);

    // dump registers onto stack and scan the stack
    void (*volatile _mark_stack)(volatile struct shard_gc*) = mark_stack;
    jmp_buf ctx;
    memset(&ctx, 0, sizeof(jmp_buf));
    setjmp(ctx);
    _mark_stack(gc);
}

static size_t sweep(volatile struct shard_gc* gc) {
    DBG_PRINTF("initiating GC sweep (gc@%p)\n", (void*) gc);
    size_t total = 0;

    for(size_t i = 0; i < gc->allocs->capacity; i++) {
        struct gc_allocation* chunk = gc->allocs->allocs[i];
        struct gc_allocation* next = NULL;

        while(chunk) {
            if(chunk->tag & GC_TAG_MARK) {
                DBG_PRINTF("found used allocation %p (ptr=%p)\n", (void*) chunk, (void*) chunk->ptr);
                // unmark
                chunk->tag &= ~GC_TAG_MARK;
                chunk = chunk->next;
            }
            else {
                DBG_PRINTF("found unused allocation %p (%lu bytes @ ptr=%p)\n", (void*) chunk, chunk->size, chunk->ptr);
                // delete chunk
                total += chunk->size;
                if(chunk->dtor) 
                    chunk->dtor(chunk->ptr);
                gc->ctx->free(chunk->ptr);

                next = chunk->next;
                gc_allocation_map_remove(gc->ctx, gc->allocs, chunk->ptr, false);
                chunk = next;
            }
        }
    }

    gc_allocation_map_resize_to_fit(gc->ctx, gc->allocs);
    return total;
}

static void unroot(volatile struct shard_gc* gc) {
    DBG_PRINTF("Unmarking roots\n");
    
    for(size_t i = 0; i < gc->allocs->capacity; ++i) {
        struct gc_allocation* chunk = gc->allocs->allocs[i];
        while(chunk) {
            if(chunk->tag & GC_TAG_ROOT)
                chunk->tag &= ~GC_TAG_ROOT;
            chunk = chunk->next;
        }
    }
}

void shard_gc_end(struct shard_gc *gc) {
    unroot(gc);
    sweep(gc);
    gc_allocation_map_delete(gc->ctx, gc->allocs);
    gc->ctx->free(gc);
}

void shard_gc_run(volatile struct shard_gc* gc)
{
    DBG_PRINTF("initiating GC run (gc@%p)\n", (void*) gc);
    mark(gc);
    sweep(gc);
}

char* shard_gc_strdup(volatile struct shard_gc* gc, const char* str, size_t size) {
    char* dup = shard_gc_malloc(gc, size + 1);
    memcpy(dup, str, size);
    dup[size] = '\0';
    return dup;
}

#endif /* SHARD_USE_GCBOEHM */

