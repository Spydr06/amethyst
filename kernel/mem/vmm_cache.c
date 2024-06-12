#include <mem/vmm.h>

#include <filesystem/virtual.h>
#include <sys/mutex.h>
#include <sys/hash.h>

#include <assert.h>
#include <string.h>

#define TABLE_SIZE 4096

static mutex_t mutex;
static semaphore_t sync;

static struct page** table;

static uintptr_t get_entry(struct vnode* vnode, uintptr_t offset) {
    struct {
        struct vnode* vnode;
        uintptr_t offset;
    } tmp = { vnode, offset };

    return fnv1ahash(&tmp, sizeof(tmp)) % TABLE_SIZE;
}

static struct page* find_page(struct vnode* vnode, uintptr_t offset) {
    uintptr_t entry = get_entry(vnode, offset);

    struct page* page = table[entry];
    while(page) {
        if(page->backing == vnode && page->offset == offset)
            break;
        page = page->hash_next;
    }

    return page;
}

int vmm_cache_get_page(struct vnode* vnode, uintptr_t offset, struct page** res) {
    assert(vnode->type == V_TYPE_REGULAR || vnode->type == V_TYPE_BLKDEV);
    assert((offset % PAGE_SIZE) == 0);

retry:
    mutex_acquire(&mutex, false);
    
    struct page* new_page = nullptr;
    volatile struct page* page = find_page(vnode, offset);

    unimplemented();
}

void vmm_cache_init(void) {
    mutex_init(&mutex);
    
    table = vmm_map(nullptr, TABLE_SIZE * sizeof(struct page), VMM_FLAGS_ALLOCATE, MMU_FLAGS_READ | MMU_FLAGS_WRITE | MMU_FLAGS_NOEXEC, nullptr);
    assert(table);
    memset(table, 0, TABLE_SIZE * sizeof(struct page));

    semaphore_init(&sync, 0);

    // TODO: sync thread...
}
