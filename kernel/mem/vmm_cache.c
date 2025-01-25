#include <mem/vmm.h>

#include <mem/page.h>
#include <filesystem/virtual.h>
#include <sys/timekeeper.h>
#include <sys/mutex.h>
#include <sys/hash.h>

#include <assert.h>
#include <string.h>
#include <errno.h>

#define TABLE_SIZE 4096

static mutex_t mutex;
static semaphore_t sync;

static struct page** table;

static struct page* dirty_pages;

size_t cached_pages = 0;

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

static void put_page(struct page* page) {
    uintmax_t entry = get_entry(page->backing, page->offset);

    page->hash_prev = nullptr;
    page->hash_next = table[entry];
    if(table[entry])
        table[entry]->hash_prev = page;

    table[entry] = page;

    page->vnode_prev = nullptr;
    page->vnode_next = page->backing->pages;

    if(page->backing->pages)
        page->backing->pages->vnode_prev = page;

    page->backing->pages = page;

    cached_pages++;
}

static void remove_page(struct page* page) {
    uintmax_t entry = get_entry(page->backing, page->offset);

    // remove from table
    if(page->hash_next)
        page->hash_next->hash_prev = page->hash_prev;
    
    if(page->hash_prev)
        page->hash_prev->hash_next = page->hash_next;
    else
        table[entry] = page->hash_next;

    page->hash_next = nullptr;
    page->hash_prev = nullptr;

    // remove from vnode
    if(page->vnode_next)
        page->vnode_next->vnode_prev = page->vnode_prev;

    if(page->vnode_prev)
        page->vnode_prev->vnode_next = page->vnode_next;
    else
        page->backing->pages = page->vnode_next;

    page->vnode_next = nullptr;
    page->vnode_prev = nullptr;

    cached_pages--;
}

int vmm_cache_truncate(struct vnode* vnode __unused, uintmax_t offset __unused) {
    unimplemented();
}

int vmm_cache_get_page(struct vnode* vnode, uintptr_t offset, struct page** res) {
    assert(vnode->type == V_TYPE_REGULAR || vnode->type == V_TYPE_BLKDEV);
    assert((offset % PAGE_SIZE) == 0);

retry:
    mutex_acquire(&mutex, false);
    
    struct page* new_page = nullptr;
    volatile struct page* page = find_page(vnode, offset);

    if(page) {
        page_hold((struct page*) page);
        mutex_release(&mutex);

        if(new_page)
            page_release(new_page);

        // FIXME: TODO: wait for page update

        if(page->flags & PAGE_FLAGS_ERROR) {
            page_release((struct page*) page);
            goto retry;
        }

        *res = (struct page*) page;
    }
    else {
        mutex_release(&mutex);

        void* addr = pmm_alloc_page(PMM_SECTION_DEFAULT);
        if(!addr)
            return ENOMEM;

        new_page = vmm_alloc_page_meta(addr);

        mutex_acquire(&mutex, false);

        page = find_page(vnode, offset);
        if(page)
            goto retry;

        new_page->backing = vnode;
        new_page->offset = offset;

        put_page(new_page);

        mutex_release(&mutex);

        int err = vop_getpage(vnode, offset, new_page);
        if(err) {
            mutex_acquire(&mutex, false);

            remove_page(new_page);

            new_page->flags |= PAGE_FLAGS_ERROR;
            new_page->backing = nullptr;
            new_page->offset = 0;

            mutex_release(&mutex);
            page_release(new_page);
            // TODO: signal event
            return err;
        }

        new_page->flags |= PAGE_FLAGS_READY;
        // TODO: signal event
        *res = new_page;
    }

    mutex_release(&mutex);
    return 0;
}

int vmm_cache_make_dirty(struct page* page) {
    bool dirty = false;

    mutex_acquire(&mutex, false);

    if(!(page->flags & (PAGE_FLAGS_DIRTY | PAGE_FLAGS_TRUNCATED))) {
        dirty = true;
        page->flags |= PAGE_FLAGS_DIRTY;

        page->write_prev = nullptr;
        page->write_next = dirty_pages;

        if(dirty_pages)
            dirty_pages->write_prev = page;

        dirty_pages = page;

        page_hold(page);
        assert(page->backing);
        vop_hold(page->backing);
    }

    mutex_release(&mutex);

    if(dirty) {
        struct vattr attr;
        attr.mtime = timekeeper_time();
        vop_setattr(page->backing, &attr, V_ATTR_MTIME, nullptr);
    }

    return 0;
}

void vmm_cache_init(void) {
    mutex_init(&mutex);
    
    table = vmm_map(nullptr, TABLE_SIZE * sizeof(struct page), VMM_FLAGS_ALLOCATE, MMU_FLAGS_READ | MMU_FLAGS_WRITE | MMU_FLAGS_NOEXEC, nullptr);
    assert(table);
    memset(table, 0, TABLE_SIZE * sizeof(struct page));

    semaphore_init(&sync, 0);

    // TODO: sync thread...
}

