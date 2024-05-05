#include <mem/slab.h>
#include <mem/vmm.h>

#include <math.h>
#include <kernelio.h>
#include <assert.h>

#define SLAB_PAGE_OFFSET (PAGE_SIZE - sizeof(struct slab))
#define SLAB_DATA_SIZE SLAB_PAGE_OFFSET

#define SLAB_INDIRECT_CUTOFF 512
#define SLAB_INDIRECT_COUNT 16

#define GET_SLAB(x) ((struct slab*) (ROUND_DOWN((uintptr_t) (x), PAGE_SIZE) + SLAB_PAGE_OFFSET))

static struct scache self_cache = {
    .size = sizeof(struct scache),
    .align = 8,
    .true_size = ROUND_UP(sizeof(struct scache) + sizeof(void**), 8),
    .slab_obj_count = SLAB_DATA_SIZE / ROUND_UP(sizeof(struct scache) + sizeof(void**), 8)
};

static void init_direct(struct scache* cache, struct slab* slab, void* base) {
	slab->free = nullptr;
	slab->used = 0;

	for (uintmax_t offset = 0; offset < cache->true_size*  cache->slab_obj_count; offset += cache->true_size) {
		if (cache->ctor)
			cache->ctor(cache, (void*)((uintptr_t)base + offset));

		void** freenext = (void**)((uintptr_t)base + offset + cache->size);
		*freenext = slab->free;
		slab->free = freenext;
	}
}

static void init_indirect(struct scache* cache, struct slab* slab, void** base, void* objbase) {
	slab->free = nullptr;
	slab->used = 0;
	slab->base = objbase;

	for (uintmax_t offset = 0; offset < cache->true_size*  cache->slab_obj_count; offset += cache->true_size) {
		if (cache->ctor)
			cache->ctor(cache, (void*)((uintptr_t)objbase + offset));

		void** freenext = &base[offset / cache->true_size];
		*freenext =  slab->free;
		slab->free = freenext;
	}
}

static bool growcache(struct scache* cache) {
	void* _slab = vmm_map(nullptr, PAGE_SIZE, VMM_FLAGS_ALLOCATE, MMU_FLAGS_READ | MMU_FLAGS_WRITE | MMU_FLAGS_NOEXEC, nullptr);
	if (_slab == nullptr)
		return false;
	struct slab* slab = GET_SLAB(_slab);

	if (cache->size < SLAB_INDIRECT_CUTOFF) {
		init_direct(cache, slab, _slab);
	} else {
		void* base = vmm_map(nullptr, cache->slab_obj_count * cache->true_size, VMM_FLAGS_ALLOCATE, MMU_FLAGS_READ | MMU_FLAGS_WRITE | MMU_FLAGS_NOEXEC, nullptr);
		if (base == nullptr) {
			vmm_unmap(_slab, PAGE_SIZE, 0);
			return false;
		}
		init_indirect(cache, slab, _slab, base);
	}


	slab->next = cache->empty;
	slab->prev = nullptr;
	if (slab->next)
		slab->next->prev = slab;
	cache->empty = slab;
	return true;
}

static void* takeobject(struct scache* cache, struct slab* slab) {
	if (slab->free == nullptr)
		return nullptr;

	void** objend = slab->free;

	slab->free = *slab->free;
	slab->used += 1;
	*objend = nullptr;
	if (cache->size < SLAB_INDIRECT_CUTOFF)
		return (void*)((uintptr_t)objend - cache->size);
	else
		return (void*)((uintptr_t)slab->base + ((uintptr_t)objend - ROUND_DOWN((uintptr_t)slab, PAGE_SIZE)) / sizeof(void**)*  cache->true_size);
}

// frees an object and returns the slab the object belongs to
static struct slab* returnobject(struct scache* cache, void* obj) {
	struct slab* slab = nullptr;
	void** freeptr = nullptr;
	if (cache->size < SLAB_INDIRECT_CUTOFF) {
		slab = (struct slab*)(ROUND_DOWN((uintptr_t)obj, PAGE_SIZE) + SLAB_PAGE_OFFSET);
		freeptr = (void**)((uintptr_t)obj + cache->size);
		assert(*freeptr == nullptr);
	} else {
		bool partial = cache->full == nullptr;
		slab = partial ? cache->partial : cache->full;
		while (slab) {
			void* top = (void*)((uintptr_t)slab->base + cache->slab_obj_count*  cache->true_size);
			if (obj >= slab->base && obj < top)
				break;
			if (slab->next == nullptr && partial == false) {
				slab = cache->partial;
				partial = true;
			} else {
				slab = slab->next;
			}
		}
		assert(slab);
		uintmax_t objn = ((uintptr_t)obj - (uintptr_t)slab->base) / cache->true_size;
		void** base = (void**)ROUND_DOWN((uintptr_t)slab, PAGE_SIZE);
		freeptr = &base[objn];
	}

	if (cache->dtor)
		cache->dtor(cache, obj);

	*freeptr = slab->free;
	slab->free = freeptr;
	--slab->used;

	return slab;
}

void* slab_alloc(struct scache* cache) {
	spinlock_acquire(&cache->lock);
	struct slab* slab = nullptr;
	if (cache->partial != nullptr)
		slab = cache->partial;
	else if (cache->empty != nullptr)
		slab = cache->empty;

	void* ret = nullptr;

	if (slab == nullptr) {
		if (growcache(cache))
			slab = cache->empty;
		else
			goto cleanup;
	}

	ret = takeobject(cache, slab);

	if (slab == cache->empty) {
		cache->empty = slab->next;
		if (slab->next)
			slab->next->prev = nullptr;

		if (cache->partial)
			cache->partial->prev = slab;

		slab->next = cache->partial;
		slab->prev = nullptr;
		cache->partial = slab;
	} else if (slab->used == cache->slab_obj_count) {
		cache->partial = slab->next;

		if (slab->next)
			slab->next->prev = nullptr;

		if (cache->full)
			cache->full->prev = slab;

		slab->next = cache->full;
		slab->prev = nullptr;
		cache->full = slab;
	}

	cleanup:
	spinlock_release(&cache->lock);
	return ret;
}

void slab_free(struct scache* cache, void* addr) {
	spinlock_acquire(&cache->lock);

	struct slab* slab = returnobject(cache, addr);
	assert(slab);

	if (slab->used == 0) {
		if (slab->prev == nullptr)
			cache->partial = slab->next;
		else
			slab->prev->next = slab->next;

		if (slab->next)
			slab->next->prev = slab->prev;

		slab->next = cache->empty;
		slab->prev = nullptr;
		if (slab->next)
			slab->next->prev = slab;
		cache->empty = slab;
	}
	
	if (slab->used == cache->slab_obj_count - 1) {
		if (slab->prev == nullptr)
			cache->full = slab->next;
		else
			slab->prev->next = slab->next;

		if (slab->next)
			slab->next->prev = slab->prev;

		slab->next = cache->partial;
		slab->prev = nullptr;
		if (slab->next)
			slab->next->prev = slab;
		cache->partial = slab;
	}
	spinlock_release(&cache->lock);
}

struct scache* slab_newcache(size_t size, size_t align, void (*ctor)(struct scache* , void*), void (*dtor)(struct scache* , void*)) {
	if (align == 0)
		align = 8;

	struct scache* cache = slab_alloc(&self_cache);
	if (cache == nullptr)
		return nullptr;

	cache->size = size;
	cache->align = align;
	size_t freeptrsize = size < SLAB_INDIRECT_CUTOFF ? sizeof(void**) : 0;
	cache->true_size = ROUND_UP(size + freeptrsize, align);
	cache->ctor = ctor;
	cache->dtor = dtor;
	cache->slab_obj_count = size < SLAB_INDIRECT_CUTOFF ? SLAB_DATA_SIZE / cache->true_size : SLAB_INDIRECT_COUNT;
	cache->full = nullptr;
	cache->empty = nullptr;
	cache->partial = nullptr;
	spinlock_init(cache->lock);

	klog(WARN, "new cache: size: %lu -- align: %lu -- true_size: %lu -- objcount: %lu", cache->size, cache->align, cache->true_size, cache->slab_obj_count);

	return cache;
}

static size_t purge(struct scache* cache, size_t maxcount){
	struct slab* slab = cache->empty;
	for (size_t done = 0; done < maxcount; ++done) {
		if (slab == nullptr)
			return done;

		struct slab* next = slab->next;
		if (next)
			next->prev = nullptr;

		if (cache->size >= SLAB_INDIRECT_CUTOFF)
			vmm_unmap(slab->base, cache->slab_obj_count * cache->true_size, 0);

		vmm_unmap(slab, PAGE_SIZE, 0);

		slab = cache->empty;
		cache->empty = next;
	}

	return maxcount;
}

void slab_freecache(struct scache* cache) {
	spinlock_acquire(&cache->lock);
	assert(cache->partial == nullptr);
	assert(cache->full == nullptr);

	purge(cache, (size_t)-1);

	slab_free(&self_cache, cache);
}
