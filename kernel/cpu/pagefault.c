#include <cpu/interrupts.h>

#include <sys/thread.h>
#include <sys/proc.h>
#include <mem/vmm.h>
#include <mem/pmm.h>
#include <mem/page.h>
#include <mem/user.h>

#include <assert.h>
#include <errno.h>
#include <kernelio.h>
#include <math.h>
#include <memory.h>

static bool handle_pagefault(void* addr, bool user, enum vmm_action actions) {
    if(!user && addr > USERSPACE_END)
        return false;

    addr = (void*) ROUND_DOWN((uintptr_t) addr, PAGE_SIZE);
    struct vmm_space* space = vmm_get_space(addr);

    if(!space || (space == &vmm_kernel_space && user)) {
        klog(ERROR, "no such space or space accessed in kernel\n");
        return false;
    }

    interrupt_lower_ipl(IPL_NORMAL);

    mutex_acquire(&space->lock, false);

    bool handled = false;
    struct vmm_range* range = vmm_get_range(space, addr);
    if(!range)
        goto cleanup;

    enum vmm_action invalid_actions = 0;
    if(!(range->mmu_flags & MMU_FLAGS_READ))
        invalid_actions |= VMM_ACTION_READ;
    if(!(range->mmu_flags & MMU_FLAGS_WRITE))
        invalid_actions |= VMM_ACTION_WRITE;
    if(range->mmu_flags & MMU_FLAGS_NOEXEC)
        invalid_actions |= VMM_ACTION_EXEC;

    if(invalid_actions & actions) {
        char invalid_str[4], action_str[4];
        vmm_action_as_str(invalid_actions, invalid_str);
        vmm_action_as_str(actions, action_str);

        klog(ERROR, "memory actions are not disjunct: %s <> %s", invalid_str, action_str);
        goto cleanup;
    }

    struct thread* thread = current_thread();
    struct proc* proc = thread ? thread->proc : nullptr;
    struct cred* cred = proc ? &proc->cred : nullptr;

    if(!mmu_is_present(thread->vmm_context->page_table, addr)) {
        uintmax_t map_offset = (uintptr_t) addr - (uintptr_t) range->start;
        
        if(!vfs_is_cacheable(range->vnode)) {
            // page not present in page tables
            if(range->flags & VMM_FLAGS_FILE) {
                vop_lock(range->vnode);
                assert(vop_mmap(range->vnode, addr, range->offset + map_offset, V_FFLAGS_READ | mmu_to_vnode_flags(range->mmu_flags) | (range->flags & VMM_FLAGS_SHARED ? V_FFLAGS_SHARED : 0), cred) == 0);
                vop_unlock(range->vnode);
                handled = true;
            }
            
            goto cleanup;
        }
        
        struct page* page = nullptr;
        int err = vmm_cache_get_page(range->vnode, range->offset + map_offset, &page);
        if(err) {
            handled = false;
            klog(ERROR, "vmm_cache_get_page() returned %d", err);
            goto cleanup;
        }

        handled = mmu_map(current_vmm_context()->page_table, page_get_physical(page), addr, range->mmu_flags & ~MMU_FLAGS_WRITE);
        if(!handled) {
            klog(ERROR, "could not map file into address space: Out of Memory");
            page_release(page);
        }
    }
    else if(!mmu_is_writable(thread->vmm_context->page_table, addr)) {
        // page present but not writable        
        void* old_phys = mmu_get_physical(current_vmm_context()->page_table, addr);
        
        // TODO: remap page if it is part of shared memory / shard files

        // copy on write
        void* new_phys = pmm_alloc_page(PMM_SECTION_DEFAULT);
        if(new_phys) {
            memcpy(MAKE_HHDM(new_phys), MAKE_HHDM(old_phys), PAGE_SIZE);
            mmu_remap(current_vmm_context()->page_table, new_phys, addr, range->mmu_flags);
            mmu_invalidate_range(addr, PAGE_SIZE);
            handled = true;
        }
    }
    else
        handled = true;


cleanup:
    mutex_release(&space->lock);

    return handled;
}

static void pagefault_interrupt(struct cpu_context* status) {
    struct thread* thread = current_thread();

    interrupt_set(true);

    bool in_userspace = status->cs != 8;
    enum vmm_action action = violation_to_vmm_action(status->error_code);

    char perms[4];
    vmm_action_as_str(action, perms);

    // klog(ERROR, "PF @ %p: %s [%d]", (void*) status->cr2, perms, in_userspace);

    if(handle_pagefault((void*) status->cr2, in_userspace, action))
        return;

    if(thread && thread->user_memcpy_context) {
        memcpy(status, thread->user_memcpy_context, sizeof(struct cpu_context));
        thread->user_memcpy_context = nullptr;
        CPU_RET(status) = EFAULT;
    }
    // TODO: signal user proc
    else {
        char perms[4];
        vmm_action_as_str(action, perms);
        
        panic_r(status, "\n+++ Page Fault at %p accessing %p (\"%s\", %s, cpu %d / tid %d / pid %d)", 
                (void*) status->rip,
                (void*) status->cr2, 
                perms, 
                in_userspace ? "user" : "kernel",
                _cpu()->id,
                current_thread() ? current_thread()->tid : -1,
                current_proc() ? current_proc()->pid : -1);
    }
}

void pagefault_init(void) {
    interrupt_register(0x0e, pagefault_interrupt, nullptr, IPL_IGNORE);
}

