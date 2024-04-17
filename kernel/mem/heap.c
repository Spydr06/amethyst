#include "mem/mmap.h"
#include <mem/heap.h>

#include <mem/vmm.h>
#include <kernelio.h>

#define ALIGN(size) (((size) / KERNEL_HEAP_ALLOC_ALIGN + 1) * KERNEL_HEAP_ALLOC_ALIGN)

static struct kheap_node *heap_start, *heap_current_pos, *heap_end;

static struct kheap_node* new_heap_node(struct kheap_node* current_node, size_t size);
static void expand_heap(size_t size);
static enum kheap_merge can_merge(struct kheap_node* node);
static void merge_memory_nodes(struct kheap_node* left, struct kheap_node* right);

void kernel_heap_init(void) {
    uint64_t* kheap_vaddress = vmm_alloc(PAGE_SIZE, VMM_FLAGS_PRESENT | VMM_FLAGS_WRITE_ENABLE, nullptr);
    if(!kheap_vaddress)
        panic("Could not allocate heap");


    heap_start = (struct kheap_node*) kheap_vaddress;

    heap_current_pos = heap_start;
    heap_end = heap_start;

    heap_current_pos->size = KERNEL_HEAP_INITIAL_SIZE;
    heap_current_pos->is_free = true;
    heap_current_pos->next = nullptr;
    heap_current_pos->prev = nullptr;
}

void* kmalloc(size_t size) {
    if(!size)
        return nullptr;

    struct kheap_node* current_node = heap_start;

    while(current_node) {
        size_t real_size = ALIGN(size + sizeof(struct kheap_node));
        if(current_node->is_free && current_node->size >= real_size) {
            if(current_node->size - real_size > KERNEL_HEAP_MIN_ALLOC_SIZE) {
                new_heap_node(current_node, real_size);
                current_node->is_free = false;
                current_node->size = real_size;
            }
            else
                current_node->is_free = false;
            return ((void*) current_node) + sizeof(struct kheap_node);
        }

        if(current_node == heap_end) {
            expand_heap(real_size);
            if(current_node->prev)
                current_node = current_node->prev;
        }
        current_node = current_node->next;
    }

    return nullptr;
}

void* krealloc(void* ptr, size_t size) {
    unimplemented();
}

void kfree(void* ptr) {
    if(!ptr || (uintptr_t) ptr < (uintptr_t) heap_start || (uintptr_t) ptr > (uintptr_t) heap_end)
        return;

    struct kheap_node* current_node = heap_start;
    while(current_node) {
        if(((uintptr_t) current_node) + sizeof(struct kheap_node) == (uintptr_t) ptr) {
            current_node->is_free = true;
            enum kheap_merge available_merges = can_merge(current_node);

            if(available_merges & KHEAP_MERGE_RIGHT)
                merge_memory_nodes(current_node, current_node->next);

            if(available_merges & KHEAP_MERGE_LEFT)
                merge_memory_nodes(current_node->prev, current_node);

            return;
        }

        current_node = current_node->next;
    }
}

static struct kheap_node* new_heap_node(struct kheap_node* current_node, size_t size) {
    size_t header_size = sizeof(struct kheap_node);
    struct kheap_node* new_node = (struct kheap_node*) (((void*) current_node) + sizeof(struct kheap_node));
    new_node->is_free = true;
    new_node->size = current_node->size - (size + header_size);
    new_node->prev = current_node;
    new_node->next = current_node->next;
    
    if(current_node->next)
        current_node->next->prev = new_node;

    current_node->next = new_node;

    if(current_node == heap_end)
        heap_end = new_node;

    return new_node;
}

static uintptr_t get_kheap_end(void) {
    return ((uintptr_t) heap_end) + heap_end->size + sizeof(struct kheap_node);
}

static void expand_heap(size_t required_size) {
    size_t num_pages = align_value_to_page(required_size) / PAGE_SIZE; 
    uintptr_t heap_end_addr = get_kheap_end();
    if(heap_end_addr > end_of_mapped_memory)
        map_vaddress_range((uint64_t*) heap_end_addr, VMM_FLAGS_WRITE_ENABLE | VMM_FLAGS_PRESENT, num_pages, nullptr);
    
    struct kheap_node* new_tail = (struct kheap_node*) heap_end;
    new_tail->next = nullptr;
    new_tail->prev = heap_end;
    new_tail->size = PAGE_SIZE * num_pages;
    new_tail->is_free = true;
    heap_end->next = new_tail;
    heap_end = new_tail;

    if(can_merge(new_tail) & KHEAP_MERGE_LEFT)
        merge_memory_nodes(new_tail->prev, new_tail); 
}

static enum kheap_merge can_merge(struct kheap_node* node) {
    struct kheap_node* prev = node->prev;
    struct kheap_node* next = node->next;

    enum kheap_merge available_merges = 0;
    if(prev && prev->is_free) {
        uintptr_t prev_address = ((uintptr_t) prev) + sizeof(struct kheap_node) + prev->size;
        if(prev_address == (uintptr_t) node)
            available_merges |= KHEAP_MERGE_LEFT;
    }

    if(next && next->is_free) {
        uintptr_t next_address = ((uintptr_t) node) + sizeof(struct kheap_node) + node->size;
        if(next_address == (uintptr_t) node->next)
            available_merges |= KHEAP_MERGE_RIGHT;
    }

    return available_merges;
}

static void merge_memory_nodes(struct kheap_node* left, struct kheap_node* right) {
    if(!left || !right || ((uintptr_t) left) + left->size + sizeof(struct kheap_node) != (uintptr_t) right)
        return;

    left->size = left->size + right->size + sizeof(struct kheap_node);
    left->next = right->next;
    if(right->next)
        right->next->prev = left;
}

