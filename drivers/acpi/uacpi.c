#include "mem/vmm.h"
#include "sys/mutex.h"
#include "sys/scheduler.h"
#include "sys/timekeeper.h"
#include "uacpi/log.h"
#include "uacpi/types.h"
#include <uacpi/status.h>
#include <uacpi/uacpi.h>
#include <uacpi/event.h>
#include <uacpi/kernel_api.h>

#include <mem/heap.h>
#include <mem/pmm.h>
#include <mem/vmm.h>

#include <kernelio.h>
#include <errno.h>
#include <assert.h>
#include <math.h>

#ifdef __x86_64__
    #include <x86_64/cpu/acpi.h>
#endif

uacpi_status uacpi_kernel_get_rsdp(uacpi_phys_addr *out_rsdp_address) {
    *out_rsdp_address = (uacpi_phys_addr) FROM_HHDM(acpi_get_rsdp());
    return UACPI_STATUS_OK;
}

void *uacpi_kernel_map(uacpi_phys_addr addr, uacpi_size len) {
    uintmax_t offset = (uintptr_t) addr % PAGE_SIZE;

    void *virt = vmm_map(NULL, ROUND_UP(len + offset, PAGE_SIZE), VMM_FLAGS_PHYSICAL, MMU_FLAGS_READ | MMU_FLAGS_WRITE | MMU_FLAGS_NOEXEC, (void*) ROUND_DOWN(addr, PAGE_SIZE));

    assert(virt != NULL);
    return (void*)((uintptr_t) virt + offset);
}

void uacpi_kernel_unmap(void *addr, uacpi_size len) {
    klog(DEBUG, "uacpi_kernel_unmap(%p, %zu)", addr, len);
    uintmax_t offset = (uintptr_t) addr % PAGE_SIZE;

//    vmm_unmap((void*) ROUND_DOWN((uintptr_t) addr, PAGE_SIZE), ROUND_UP(len + offset, PAGE_SIZE), 0);
}

static inline enum klog_severity uacpi_log_level_to_severity(enum uacpi_log_level level) {
    switch(level) {
    case UACPI_LOG_DEBUG:
        return KLOG_DEBUG;
    case UACPI_LOG_TRACE:
        return KLOG_INFO;
    case UACPI_LOG_INFO:
        return KLOG_INFO;
    case UACPI_LOG_WARN:
        return KLOG_WARN;
    case UACPI_LOG_ERROR:
        return KLOG_ERROR;
    default:
        return KLOG_INFO;
    }
}

#ifndef UACPI_FORMATTED_LOGGING
void uacpi_kernel_log(uacpi_log_level level, const uacpi_char* msg) {
    if(!msg)
        return;

    __klog_inl(uacpi_log_level_to_severity(level), "[uACPI]", "%s", msg);
}
#else
void uacpi_kernel_log(uacpi_log_level level, const uacpi_char* fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    uacpi_kernel_vlog(level, fmt, ap);
    va_end(ap);
}

void uacpi_kernel_vlog(uacpi_log_level level, const uacpi_char* fmt, uacpi_va_list ap) {
    __vklog_inl(uacpi_log_level_to_severity(level), "[uACPI]", fmt, ap);
}
#endif

uacpi_status uacpi_kernel_pci_device_open(
    uacpi_pci_address address, uacpi_handle *out_handle
) {
    unimplemented();
}

void uacpi_kernel_pci_device_close(uacpi_handle) {
    unimplemented();
}

uacpi_status uacpi_kernel_pci_read8(
    uacpi_handle device, uacpi_size offset, uacpi_u8 *value
) {
    unimplemented();
}

uacpi_status uacpi_kernel_pci_read16(
    uacpi_handle device, uacpi_size offset, uacpi_u16 *value
) {
    unimplemented();
}

uacpi_status uacpi_kernel_pci_read32(
    uacpi_handle device, uacpi_size offset, uacpi_u32 *value
) {
    unimplemented();
}

uacpi_status uacpi_kernel_pci_write8(
    uacpi_handle device, uacpi_size offset, uacpi_u8 value
) {
    unimplemented();
}

uacpi_status uacpi_kernel_pci_write16(
    uacpi_handle device, uacpi_size offset, uacpi_u16 value
) {
    unimplemented();
}

uacpi_status uacpi_kernel_pci_write32(
    uacpi_handle device, uacpi_size offset, uacpi_u32 value
) {
    unimplemented();
}

uacpi_status uacpi_kernel_io_map(
    uacpi_io_addr base, uacpi_size len, uacpi_handle *out_handle
) {
    unimplemented();
}

void uacpi_kernel_io_unmap(uacpi_handle handle) {
    unimplemented();
}

uacpi_status uacpi_kernel_io_read8(
    uacpi_handle, uacpi_size offset, uacpi_u8 *out_value
) {
    unimplemented();
}

uacpi_status uacpi_kernel_io_read16(
    uacpi_handle, uacpi_size offset, uacpi_u16 *out_value
) {
    unimplemented();
}

uacpi_status uacpi_kernel_io_read32(
    uacpi_handle, uacpi_size offset, uacpi_u32 *out_value
) {
    unimplemented();
}

uacpi_status uacpi_kernel_io_write8(
    uacpi_handle, uacpi_size offset, uacpi_u8 in_value
) {
    unimplemented();
}

uacpi_status uacpi_kernel_io_write16(
    uacpi_handle, uacpi_size offset, uacpi_u16 in_value
) {
    unimplemented();
}

uacpi_status uacpi_kernel_io_write32(
    uacpi_handle, uacpi_size offset, uacpi_u32 in_value
) {
    unimplemented();
}

void *uacpi_kernel_alloc(uacpi_size size) {
    return kmalloc((size_t) size);
}

void *uacpi_kernel_alloc_zeroed(uacpi_size size) {
    return kcalloc(1, size);
}

void uacpi_kernel_free(void *mem) {
    kfree(mem);
}

uacpi_u64 uacpi_kernel_get_nanoseconds_since_boot(void) {
    here();
    struct timespec ts = timekeeper_time_from_boot();
    return ts.ns + ts.s * 1'000'000ull;
}

void uacpi_kernel_stall(uacpi_u8 usec) {
    panic("UACPI STALL");
}

void uacpi_kernel_sleep(uacpi_u64 msec) {
    here();
    sched_sleep(msec * 1000ull);
}

uacpi_handle uacpi_kernel_create_mutex(void) {
    mutex_t *mut = kmalloc(sizeof(mutex_t));
    assert(mut != NULL);

    mutex_init(mut);
    return (uacpi_handle) mut;
}

void uacpi_kernel_free_mutex(uacpi_handle handle) {
    mutex_t *mut = (mutex_t*) handle;
    kfree(mut);
}

uacpi_handle uacpi_kernel_create_event(void) {
    unimplemented();
}

void uacpi_kernel_free_event(uacpi_handle) {
    unimplemented();
}

uacpi_thread_id uacpi_kernel_get_thread_id(void) {
    unimplemented();
}

uacpi_status uacpi_kernel_acquire_mutex(uacpi_handle handle, uacpi_u16) {
    here();
    // TODO: timer
    mutex_t *mut = (mutex_t*) handle;

    mutex_acquire(mut, false);
    return UACPI_STATUS_OK;
}

void uacpi_kernel_release_mutex(uacpi_handle handle) {
    mutex_t *mut = (mutex_t*) handle;

    mutex_release(mut);
}

uacpi_bool uacpi_kernel_wait_for_event(uacpi_handle, uacpi_u16) {
    unimplemented();
}

void uacpi_kernel_signal_event(uacpi_handle) {
    unimplemented();
}

void uacpi_kernel_reset_event(uacpi_handle) {
    unimplemented();
}

uacpi_status uacpi_kernel_handle_firmware_request(uacpi_firmware_request*) {
    unimplemented();
}

uacpi_status uacpi_kernel_install_interrupt_handler(
    uacpi_u32 irq, uacpi_interrupt_handler, uacpi_handle ctx,
    uacpi_handle *out_irq_handle
) {
    unimplemented();
}

uacpi_status uacpi_kernel_uninstall_interrupt_handler(
    uacpi_interrupt_handler, uacpi_handle irq_handle
) {
    unimplemented();
}

uacpi_handle uacpi_kernel_create_spinlock(void) {
    spinlock_t *lock = kmalloc(sizeof(spinlock_t));
    assert(lock != NULL);

    spinlock_init(lock); 
    return lock;
}

void uacpi_kernel_free_spinlock(uacpi_handle handle) {
    spinlock_t *lock = (spinlock_t *) handle;

    kfree(lock);
}

uacpi_cpu_flags uacpi_kernel_lock_spinlock(uacpi_handle handle) {
    here();
    spinlock_t *lock = (spinlock_t *) handle;
    spinlock_acquire(lock);

    return 0;
}

void uacpi_kernel_unlock_spinlock(uacpi_handle handle, uacpi_cpu_flags flags) {
    here();
    (void) flags;

    spinlock_t *lock = (spinlock_t *) handle;
    spinlock_release(lock); 
}

uacpi_status uacpi_kernel_schedule_work(
    uacpi_work_type, uacpi_work_handler, uacpi_handle ctx
) {
    unimplemented();
}

uacpi_status uacpi_kernel_wait_for_work_completion(void) {
    unimplemented();
}

int uacpi_init(void) {
    uacpi_status err = uacpi_initialize(0);

    if(uacpi_unlikely_error(err)) {
        klog(ERROR, "uacpi_initialize failed: %s", uacpi_status_to_string(err));
        return ENODEV;
    }

    err = uacpi_namespace_load();
    if(uacpi_unlikely_error(err)) {
        klog(ERROR, "uacpi_namespace_load failed: %s", uacpi_status_to_string(err));
        return ENODEV;
    }

    err = uacpi_namespace_initialize();
    if(uacpi_unlikely_error(err)) {
        klog(ERROR, "uacpi_namespace_initialize failed: %s", uacpi_status_to_string(err));
        return ENODEV;
    }

    err = uacpi_finalize_gpe_initialization();
    if(uacpi_unlikely_error(err)) {
        klog(ERROR, "uacpi_finalize_gpe_initialization failed: %s", uacpi_status_to_string(err));
        return ENODEV;
    }

    return 0;
}
