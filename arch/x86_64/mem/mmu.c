#include "x86_64/cpu/cpu.h"
#include "x86_64/cpu/idt.h"
#include "x86_64/dev/pic.h"
#include <x86_64/mem/mmu.h>

#include <mem/pmm.h>

#include <limine/limine.h>

#include <assert.h>
#include <string.h>
#include <kernelio.h>

#define ADDRMASK 0x7ffffffffffff000ul
#define   PTMASK 0b111111111000000000000ul
#define   PDMASK 0b111111111000000000000000000000ul
#define PDPTMASK 0b111111111000000000000000000000000000000ul
#define PML4MASK 0b111111111000000000000000000000000000000000000000ul

#define INTERMEDIATE_FLAGS (MMU_FLAGS_WRITE | MMU_FLAGS_READ | MMU_FLAGS_USER)

enum paging_depth : int8_t {
    DEPTH_PML4 = 0,
    DEPTH_PDPT = 1,
    DEPTH_PD = 2,
    DEPTH_PT = 3
};

static page_table_ptr_t template;

extern char _TEXT_START_[],   _TEXT_END_[],
            _DATA_START_[],   _DATA_END_[],
            _RODATA_START_[], _RODATA_END_[],
            _BSS_START_[], _BSS_END_[];

static void* kernel_addr[] = {
    &_TEXT_START_,
    &_TEXT_END_,
    &_DATA_START_, // FIXME: .data and .bss must follow each other!
    &_BSS_END_,
    &_RODATA_START_,
    &_RODATA_END_,
//    &_BSS_START_,
//    &_BSS_END_
};

static_assert(__len(kernel_addr) % 2 == 0);

static enum mmu_flags kernel_flags[] = {
    MMU_FLAGS_READ,
    MMU_FLAGS_READ | MMU_FLAGS_WRITE | MMU_FLAGS_NOEXEC,
    MMU_FLAGS_READ | MMU_FLAGS_NOEXEC,
//    MMU_FLAGS_READ | MMU_FLAGS_WRITE | MMU_FLAGS_NOEXEC
};

static_assert(__len(kernel_flags) == __len(kernel_addr) / 2);

static volatile struct limine_kernel_address_request kernel_address_request = {
    .id = LIMINE_KERNEL_ADDRESS_REQUEST,
    .revision = 0
};

static void* mmu_page = nullptr;
static int remaining = 0;

static __always_inline void* next(uint64_t entry) {
    return entry ? MAKE_HHDM(entry & ADDRMASK) : nullptr;
}

static bool add_page(page_table_ptr_t top, void* vaddr, uint64_t entry, enum paging_depth depth) {
    uint64_t* pml4 = MAKE_HHDM(top);
    uintptr_t addr = (uintptr_t) vaddr;
    uintptr_t pt_offset = (addr & PTMASK) >> 12;
    uintptr_t pd_offset = (addr & PDMASK) >> 21;
    uintptr_t pdpt_offset = (addr & PDPTMASK) >> 30;
    uintptr_t pml4_offset = (addr & PML4MASK) >> 39;

    if(depth == DEPTH_PDPT) {
        pml4[pml4_offset] = entry;
        return true;
    }

    uint64_t* pdpt = next(pml4[pml4_offset]);
    if(!pdpt) {
        pdpt = pmm_alloc_page(PMM_SECTION_DEFAULT);
        if(!pdpt)
            return false;
        pml4[pml4_offset] = (uint64_t) pdpt | INTERMEDIATE_FLAGS;
        pdpt = MAKE_HHDM(pdpt);
        memset(pdpt, 0, PAGE_SIZE);
    }

    if(depth == DEPTH_PD) {
        pdpt[pdpt_offset] = entry;
        return true;
    }

    uint64_t* pd = next(pdpt[pdpt_offset]);
    if(!pd) {
        pd = pmm_alloc_page(PMM_SECTION_DEFAULT);
        if(!pd)
            return false;
        pdpt[pdpt_offset] = (uint64_t) pd | INTERMEDIATE_FLAGS;
        pd = MAKE_HHDM(pd);
        memset(pd, 0, PAGE_SIZE);
    }

    if(depth == DEPTH_PT) {
        pd[pd_offset] = entry;
        return true;
    }

    uint64_t* pt = next(pd[pd_offset]);
    if(!pt) {
        pt = pmm_alloc_page(PMM_SECTION_DEFAULT);
        if(!pt)
            return false;
        pd[pd_offset] = (uint64_t) pt | INTERMEDIATE_FLAGS;
        pt = MAKE_HHDM(pt);
        memset(pt, 0, PAGE_SIZE);
    }

    pt[pt_offset] = entry;
    return true;
}

static uint64_t* get_page(page_table_ptr_t top, void* vaddr) {
    uint64_t* pml4 = MAKE_HHDM(top);
    uintptr_t addr = (uintptr_t) vaddr;
    uintptr_t pt_offset = (addr & PTMASK) >> 12;
    uintptr_t pd_offset = (addr & PDMASK) >> 21;
    uintptr_t pdpt_offset = (addr & PDPTMASK) >> 30;
    uintptr_t pml4_offset = (addr & PML4MASK) >> 39;

    uint64_t* pdpt = next(pml4[pml4_offset]);
    if(!pdpt)
        return nullptr;

    uint64_t* pd = next(pdpt[pdpt_offset]);
    if(!pd)
        return nullptr;

    uint64_t* pt = next(pd[pd_offset]);
    if(!pt)
        return nullptr;

    return pt + pt_offset;
}

static void pfisr(struct cpu_context* status __unused) {
    // TODO
//    panic("Page fault");
    klog(ERROR, __FILE__ ":pfisr() called");
}

void mmu_tlbipi(struct cpu_context* status __unused) {
    __asm__ volatile(
        "invlpg (%%rax)"
        :: "a"(mmu_page)
    );
    __atomic_sub_fetch(&remaining, 1, __ATOMIC_SEQ_CST);
}

void mmu_apswitch(void) {
    mmu_switch(FROM_HHDM(template));
    interrupt_register(0x0e, pfisr, nullptr, IPL_NORMAL);
    interrupt_register(0xfe, mmu_tlbipi, pic_send_eoi, IPL_NORMAL);
    idt_reload();
}

void mmu_init(struct mmap* mmap) {
    template = pmm_alloc_page(PMM_SECTION_DEFAULT);
    assert(template);

    template = MAKE_HHDM(template);
    memset(template, 0, PAGE_SIZE);

    for(unsigned i = 256; i < 512; i++) {
        uint64_t* entry = pmm_alloc_page(PMM_SECTION_DEFAULT);
        assert(entry);
        memset(MAKE_HHDM(entry), 0, PAGE_SIZE);
        template[i] = (uint64_t) entry | INTERMEDIATE_FLAGS;
    }

    // populate hhdm
    
    klog_inl(INFO, "hhdm mapping... [\e[%zuC]\e[%zuD", mmap->mmap->entry_count * 2, mmap->mmap->entry_count * 2 + 1);
    for(size_t i = 0; i < mmap->mmap->entry_count; i++) {
        struct limine_memmap_entry* e = mmap->mmap->entries[i];
        for(uint64_t i = 0; i < e->length; i += PAGE_SIZE) {
            uint64_t entry = ((e->base + i) & ADDRMASK) | MMU_FLAGS_READ | MMU_FLAGS_WRITE | MMU_FLAGS_NOEXEC;
            assert(add_page(FROM_HHDM(template), MAKE_HHDM(e->base + i), entry, DEPTH_PML4));
        }
        printk("==");
    }
    printk("] done.");

    // populate kernel
    klog_inl(INFO, "mapping kernel... ");
    assert(kernel_address_request.response);

    for(size_t i = 0; i < __len(kernel_addr) / 2; i++) {
        size_t len = (uintptr_t) kernel_addr[i * 2 + 1] - (uintptr_t) kernel_addr[i * 2];
        uintptr_t base_ptr = (uintptr_t) kernel_addr[i * 2];
        uintptr_t phys_base = (uintptr_t) kernel_addr[i * 2] - kernel_address_request.response->virtual_base + kernel_address_request.response->physical_base;

        for(uintptr_t off = 0; off < len; off += PAGE_SIZE) {
            uint64_t entry = ((phys_base + off) & ADDRMASK) | kernel_flags[i];
            assert(add_page(FROM_HHDM(template), (void*) (base_ptr + off), entry, 0));
        }
    }
    printk("done.");

    mmu_apswitch();
}

page_table_ptr_t mmu_new_table(void) {
    page_table_ptr_t table = pmm_alloc_page(PMM_SECTION_DEFAULT);
    if(!table)
        return nullptr;
    memcpy(MAKE_HHDM(table), template, PAGE_SIZE);
    return table;
}

static void _destroy(uint64_t* table, int depth) {
    for(size_t i = 0; i < (depth == 3 ? 256 : 512); i++) {
        void* addr = (void*) (table[i] & ADDRMASK);
        if(!addr)
            continue;

        if(depth > 0)
            _destroy(MAKE_HHDM(addr), depth - 1);

        pmm_release(addr);
    }
}

void mmu_destroy_table(page_table_ptr_t table) {
    if(!table)
        return;

    _destroy(MAKE_HHDM(table), 3);
    pmm_release(table);
}

bool mmu_map(page_table_ptr_t table, void* paddr, void* vaddr, enum mmu_flags flags) {
    uint64_t entry = ((uintptr_t) paddr & ADDRMASK) | flags;
    return add_page(table, vaddr, entry, 0);
}

void mmu_unmap(page_table_ptr_t table, void* vaddr) {
    uint64_t* entry = get_page(table, vaddr);
    if(!entry)
        return;
    *entry = 0;
    mmu_invalidate(vaddr);
}

void mmu_invalidate(void* vaddr) {
    mmu_tlb_shootdown(vaddr);
    __asm__ volatile(
        "invlpg (%%rax)"
        :: "a"(vaddr)
    );
}

void mmu_tlb_shootdown(void *page __attribute__((unused))) {
    if(_cpu()->thread == NULL)
        return;
    // TODO: check if only one cpu is active
    unimplemented();
}

void* mmu_get_physical(page_table_ptr_t table, void* vaddr) {
    uint64_t* entry = get_page(table, vaddr);
    if(!entry)
        return nullptr;
    return (void*) (*entry & ADDRMASK);
}

__noreturn void page_fault_handler(struct cpu_context* status) {
    uint64_t cr2 = 0;
    __asm__ volatile (
        "mov %%cr2, %0"
        : "=r" (cr2)
    );

    /*uint64_t cr2_offset = cr2 & VM_OFFSET_MASK;
    uint64_t pd = PD_ENTRY(cr2_offset);
    uint64_t pdpr = PDPR_ENTRY(cr2_offset);
    uint64_t pml4 = PML4_ENTRY(cr2_offset);
    uint64_t *pd_table = (uint64_t*) (SIGN_EXTENSION | ENTRIES_TO_ADDRESS(510l, 510l, (uint64_t) pml4, (uint64_t) pdpr));
    uint64_t *pdpr_table = (uint64_t*) (SIGN_EXTENSION | ENTRIES_TO_ADDRESS(510l, 510l, 510l, (uint64_t) pml4));
    uint64_t *pml4_table = (uint64_t*) (SIGN_EXTENSION | ENTRIES_TO_ADDRESS(510l, 510l, 510l, 510l));*/

    panic(
        "Page Fault at address %p:\n"
        "    Error flags: FETCH(%lu) - RSVD(%lu) - ACCESS(%lu) - WRITE(%lu) - PRESENT(%lu)"
      /*  "    Page Entries: pd: %p - pdpr: %p - pml4: %p\n"
        "                  pd  [%p] = %p\n"
        "                  pdpr[%p] = %p\n"
        "                  pml4[%p] = %p"*/,
        (void*) cr2,
        status->error_code & FETCH_VIOLATION,
        status->error_code & RESERVED_VIOLATION,
        status->error_code & ACCESS_VIOLATION,
        status->error_code & WRITE_VIOLATION,
        status->error_code & PRESENT_VIOLATION/*,
        (void*) pd, (void*) pdpr, (void*) pml4,
        (void*) pd, (void*) pd_table[pd],
        (void*) pdpr, (void*) pdpr_table[pdpr],
        (void*) pml4, (void*) pml4_table[pml4]*/
    );
}


