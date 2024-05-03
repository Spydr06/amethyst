#include <mem/pmm.h>
#include <mem/vmm.h>
#include <mem/mmap.h>
#include <sys/spinlock.h>

#include <assert.h>
#include <math.h>
#include <string.h>
#include <kernelio.h>

#include <limine/limine.h>

#define TOP_1MB (0x100000 / PAGE_SIZE)
#define TOP_4GB ((uint64_t) 0x100000000ul / PAGE_SIZE)

#define PAGE_GETID(page) (((uintptr_t)(page) - (uintptr_t) pages) / sizeof(struct page))
#define PAGE_BOUNDCHK(page_id) \
    assert((page_id) * PAGE_SIZE < (uintptr_t) pages || (page_id) * PAGE_SIZE >= (uintptr_t) &pages[page_count])

uintptr_t hhdm_base;

static volatile struct limine_hhdm_request hhdm_request = {
    .id = LIMINE_HHDM_REQUEST,
    .revision = 0
};

static size_t page_count, free_page_count;
static spinlock_t memory_spinlock;
static struct page* pages;

static struct page* free_lists[PMM_SECTION_COUNT];
static struct page* standby_lists[PMM_SECTION_COUNT];

static struct pmm_section sections[PMM_SECTION_COUNT] = {
    {0,       TOP_1MB,              0      },
    {TOP_1MB, TOP_4GB,              TOP_1MB},
    {TOP_4GB, 0xfffffffffffffffflu, TOP_4GB}
};

static void free_list_insert(struct page* page);
static void free_list_remove(struct page* page);

static void allocate(struct page* page);

void pmm_init(void) {
    assert(hhdm_request.response);
    hhdm_base = hhdm_request.response->offset;

    struct mmap mmap;
    assert(mmap_parse(&mmap) == 0);

    klog(INFO, "total memory: 0x%zx bytes", mmap.memory_size);

    pages = MAKE_HHDM(mmap.biggest_entry->base);
    page_count = ROUND_UP(mmap.top, PAGE_SIZE) / PAGE_SIZE;
    memset(pages, 0, page_count * sizeof(struct page));
    klog(INFO, "%zu pages used for page list", ROUND_UP(page_count * sizeof(struct page), PAGE_SIZE) / PAGE_SIZE);

    for(size_t i = 0; i < mmap.mmap->entry_count; i++) {
        struct limine_memmap_entry* entry = mmap.mmap->entries[i];

        if(entry->type != MMAP_AVAILABLE)
            continue;

        size_t first_usable_page = entry == mmap.biggest_entry
            ? ROUND_UP(entry->base + page_count * sizeof(struct page), PAGE_SIZE) / PAGE_SIZE
            : entry->base / PAGE_SIZE;

        for(size_t j = first_usable_page; j < (entry->base + entry->length) / PAGE_SIZE; j++) {
            pages[j].flags |= PAGE_FLAGS_FREE;
            free_list_insert(&pages[j]);
        }
    }

    spinlock_init(memory_spinlock);
}

void* pmm_alloc_page(enum pmm_section_type section) {
    spinlock_acquire(&memory_spinlock);

    struct page* page = nullptr;
    for(int i = section; i >= 0; i--) {
        page = free_lists[i];
        if(page) {
            free_list_remove(page);
            break;
        }
    }

    // TODO: get page from standby list

    spinlock_release(&memory_spinlock);

    if(!page)
        return nullptr;

    allocate(page);
    return (void*) (PAGE_GETID(page) * PAGE_SIZE);;
}

void* pmm_alloc(size_t size, enum pmm_section_type section) {
    if(!size)
        return nullptr;

    if(size == 1)
        return pmm_alloc_page(section);

    spinlock_acquire(&memory_spinlock);

    uintmax_t page = 0;
    size_t found;

    for(; section >= 0; section--) {
        found = 0;
        for(page = sections[section].search_start; page < sections[section].top_id; page++) {
            if(page >= page_count)
                break;

            if(pages[page].flags & PAGE_FLAGS_FREE)
                found++;
            else
                found = 0;

            if(found == size)
                goto got_pages;
        }
    }

    void* addr;
got_pages:
    addr = nullptr;

    if(found == size) {
        page = page - (found - 1);
        addr = (void*) (page * PAGE_SIZE);
        for(size_t i = 0; i < size; i++) {
            free_list_remove(&pages[page + i]);
            allocate(&pages[page + i]);
        }
    }

    spinlock_release(&memory_spinlock);
    return addr;
}

static void free_list_insert(struct page* page) {
    uintmax_t page_id = PAGE_GETID(page);
    PAGE_BOUNDCHK(page_id);

    enum pmm_section_type section;

    if(page_id < TOP_1MB)
        section = PMM_SECTION_1MB;
    else if(page_id < TOP_4GB)
        section = PMM_SECTION_4GB;
    else
        section = PMM_SECTION_DEFAULT;

    struct page** list = page->backing ? &standby_lists[section] : &free_lists[section];

    if(sections[section].search_start > page_id)
        sections[section].search_start = page_id;

    page->free_next = *list;
    page->free_prev = nullptr;
    *list = page;
    if(page->free_next)
        page->free_next->free_prev = page;

    free_page_count++;
}

static void free_list_remove(struct page* page) {
    uintmax_t page_id = PAGE_GETID(page);
    PAGE_BOUNDCHK(page_id);
    
    enum pmm_section_type section;

    if(page_id < TOP_1MB)
        section = PMM_SECTION_1MB;
    else if(page_id < TOP_4GB)
        section = PMM_SECTION_4GB;
    else
        section = PMM_SECTION_DEFAULT;

    struct page** list = page->backing
        ? &standby_lists[section]
        : &free_lists[section];

    if(page->free_prev)
        page->free_prev->free_next = page->free_next;
    else
        *list = page->free_next;

    if(page->free_next)
        page->free_next->free_prev = page->free_prev;

    free_page_count--;
}

static void allocate(struct page* page) {
    assert(page->refcount == 0);
    memset(page, 0, sizeof(struct page));
    page->refcount = 1;
    page->flags &= ~PAGE_FLAGS_FREE;
}


