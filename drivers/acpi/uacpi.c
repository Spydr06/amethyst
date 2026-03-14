#include "amethyst/amethyst.h"
#include "drivers/acpi/apic.h"
#include "drivers/pci/pci.h"
#include "sys/semaphore.h"
#include "sys/thread.h"
#include "x86_64/cpu/idt.h"
#include "x86_64/dev/io.h"
#include <drivers/acpi/acpi.h>
#include <drivers/acpi/tables.h>

#include <sys/mutex.h>
#include <sys/scheduler.h>
#include <sys/timekeeper.h>

#include <uacpi/event.h>
#include <uacpi/kernel_api.h>
#include <uacpi/log.h>
#include <uacpi/status.h>
#include <uacpi/types.h>
#include <uacpi/uacpi.h>

#include <mem/heap.h>
#include <mem/pmm.h>
#include <mem/vmm.h>

#include <kernelio.h>
#include <errno.h>
#include <assert.h>
#include <math.h>

uacpi_status uacpi_kernel_get_rsdp(uacpi_phys_addr *out_rsdp_address) {
    uintptr_t rsdp = acpi_get_rsdp_phys();
    if(!rsdp)
        panic("No RSDP table found.");

    *out_rsdp_address = (uacpi_phys_addr) FROM_HHDM(rsdp);
    return UACPI_STATUS_OK;
}

void *uacpi_kernel_map(uacpi_phys_addr addr, uacpi_size len) {
    // klog(DEBUG, "uacpi_kernel_map(%p, %zu)", (void*) addr, len);
    uintmax_t offset = (uintptr_t) addr % PAGE_SIZE;

    void *virt = vmm_map(nullptr, ROUND_UP(len + offset, PAGE_SIZE), VMM_FLAGS_PHYSICAL, MMU_FLAGS_READ | MMU_FLAGS_WRITE | MMU_FLAGS_NOEXEC, (void*) ROUND_DOWN(addr, PAGE_SIZE));

    assert(virt != nullptr);
    return (void*)((uintptr_t) virt + offset);
}

void uacpi_kernel_unmap(void *addr, uacpi_size len) {
    klog(WARN, "FIXME: uacpi_kernel_unmap(%p, %zu)", addr, len);
    // klog(DEBUG, "uacpi_kernel_unmap(%p, %zu)", addr, len);
    // uintmax_t offset = (uintptr_t) addr % PAGE_SIZE;

    // vmm_unmap((void*) ROUND_DOWN((uintptr_t) addr, PAGE_SIZE), ROUND_UP(len + offset, PAGE_SIZE), 0);
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

    __klog_inl(uacpi_log_level_to_severity(level), "[uacpi]", "%s", msg);
}
#else
void uacpi_kernel_log(uacpi_log_level level, const uacpi_char* fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    uacpi_kernel_vlog(level, fmt, ap);
    va_end(ap);
}

void uacpi_kernel_vlog(uacpi_log_level level, const uacpi_char* fmt, uacpi_va_list ap) {
    __vklog_inl(uacpi_log_level_to_severity(level), "[uacpi]", fmt, ap);
}
#endif

uacpi_status uacpi_kernel_pci_device_open(
    uacpi_pci_address address, uacpi_handle *out_handle
) {
    struct pci_device *dev = pci_search_device(address.segment, address.bus, address.device, address.function);
    if(!dev) {
        klog(ERROR, "requested pci device %x:%x:%x:%x not found", address.segment, address.bus, address.device, address.function);
        return UACPI_STATUS_NOT_FOUND;
    }

    *out_handle = dev;
    return UACPI_STATUS_OK;
}

void uacpi_kernel_pci_device_close(uacpi_handle handle) {
    struct pci_device *dev = (struct pci_device*) handle;
    if(!dev)
        return;

    pci_device_release(dev);
}

uacpi_status uacpi_kernel_pci_read8(
    uacpi_handle device, uacpi_size offset, uacpi_u8 *value
) {
    *value = pci_device_read_byte(device, offset);
    return UACPI_STATUS_OK;
}

uacpi_status uacpi_kernel_pci_read16(
    uacpi_handle device, uacpi_size offset, uacpi_u16 *value
) {
    *value = pci_device_read_word(device, offset);
    return UACPI_STATUS_OK;
}

uacpi_status uacpi_kernel_pci_read32(
    uacpi_handle device, uacpi_size offset, uacpi_u32 *value
) {
    *value = pci_device_read_dword(device, offset);
    return UACPI_STATUS_OK;
}

uacpi_status uacpi_kernel_pci_write8(
    uacpi_handle device, uacpi_size offset, uacpi_u8 value
) {
    pci_device_write_byte(device, offset, value);
    return UACPI_STATUS_OK;
}

uacpi_status uacpi_kernel_pci_write16(
    uacpi_handle device, uacpi_size offset, uacpi_u16 value
) {
    pci_device_write_word(device, offset, value);
    return UACPI_STATUS_OK;
}

uacpi_status uacpi_kernel_pci_write32(
    uacpi_handle device, uacpi_size offset, uacpi_u32 value
) {
    pci_device_write_dword(device, offset, value);
    return UACPI_STATUS_OK;
}

uacpi_status uacpi_kernel_io_map(
    uacpi_io_addr base, uacpi_size len, uacpi_handle *out_handle
) {
    (void) len;
    *out_handle = (uacpi_handle) base;
    return UACPI_STATUS_OK;
}

void uacpi_kernel_io_unmap(uacpi_handle handle) {
    (void) handle;
    unimplemented();
}

uacpi_status uacpi_kernel_io_read8(
    uacpi_handle handle, uacpi_size offset, uacpi_u8 *out_value
) {
    *out_value = inb((io_port_t)(uintptr_t)(handle + offset));
    return UACPI_STATUS_OK;
}

uacpi_status uacpi_kernel_io_read16(
    uacpi_handle handle, uacpi_size offset, uacpi_u16 *out_value
) {
    *out_value = inw((io_port_t)(uintptr_t)(handle + offset));
    return UACPI_STATUS_OK;
}

uacpi_status uacpi_kernel_io_read32(
    uacpi_handle handle, uacpi_size offset, uacpi_u32 *out_value
) {
    *out_value = inl((io_port_t)(uintptr_t)(handle + offset));
    return UACPI_STATUS_OK;
}

uacpi_status uacpi_kernel_io_write8(
    uacpi_handle handle, uacpi_size offset, uacpi_u8 in_value
) {
    outb((io_port_t)(uintptr_t)(handle + offset), in_value);
    return UACPI_STATUS_OK;
}

uacpi_status uacpi_kernel_io_write16(
    uacpi_handle handle, uacpi_size offset, uacpi_u16 in_value
) {
    outw((io_port_t)(uintptr_t)(handle + offset), in_value);
    return UACPI_STATUS_OK;
}

uacpi_status uacpi_kernel_io_write32(
    uacpi_handle handle, uacpi_size offset, uacpi_u32 in_value
) {
    outl((io_port_t)(uintptr_t)(handle + offset), in_value);
    return UACPI_STATUS_OK;
}

void *uacpi_kernel_alloc(uacpi_size size) {
    // klog(DEBUG, "uacpi_kernel_alloc(%zu)", size);
    return kmalloc((size_t) size);
}

void *uacpi_kernel_alloc_zeroed(uacpi_size size) {
    // klog(DEBUG, "uacpi_kernel_alloc_zeroed(%zu)", size);
    return kcalloc(1, size);
}

void uacpi_kernel_free(void *mem) {
    // klog(DEBUG, "uacpi_kernel_free(%p)", mem);
    kfree(mem);
}

uacpi_u64 uacpi_kernel_get_nanoseconds_since_boot(void) {
    struct timespec ts = timekeeper_time_from_boot();
    return ts.ns + ts.s * 1'000'000ull;
}

void uacpi_kernel_stall(uacpi_u8 usec) {
    panic("UACPI STALL: %hhu", usec);
}

void uacpi_kernel_sleep(uacpi_u64 msec) {
    sched_sleep((uintmax_t) msec * 1000ull);
}

uacpi_handle uacpi_kernel_create_mutex(void) {
    mutex_t *mut = kmalloc(sizeof(mutex_t));
    if(!mut)
        return nullptr;

    mutex_init(mut);
    return (uacpi_handle) mut;
}

void uacpi_kernel_free_mutex(uacpi_handle handle) {
    mutex_t *mut = (mutex_t*) handle;
    kfree(mut);
}

uacpi_handle uacpi_kernel_create_event(void) {
    semaphore_t *sem = kmalloc(sizeof(semaphore_t));
    if(!sem)
        return nullptr;

    semaphore_init(sem, 1);
    return sem; 
}

void uacpi_kernel_free_event(uacpi_handle handle) {
    kfree(handle);
}

uacpi_thread_id uacpi_kernel_get_thread_id(void) {
    return current_thread();
}

uacpi_status uacpi_kernel_acquire_mutex(uacpi_handle handle, uacpi_u16) {
    // TODO: timer
    mutex_t *mut = (mutex_t*) handle;
    assert(mut != nullptr);

    mutex_acquire(mut);
    return UACPI_STATUS_OK;
}

void uacpi_kernel_release_mutex(uacpi_handle handle) {
    mutex_t *mut = (mutex_t*) handle;
    assert(mut != nullptr);

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

struct acpi_interrupt {
    uacpi_interrupt_handler handler;
    uacpi_handle ctx;
};

static void acpi_irq(struct cpu_context*, void* userp) {
    struct acpi_interrupt *ai = userp;
    ai->handler(ai->ctx);
}

uacpi_status uacpi_kernel_install_interrupt_handler(
    uacpi_u32 irq, uacpi_interrupt_handler handler, uacpi_handle ctx,
    uacpi_handle *out_irq_handle
) {
    struct acpi_interrupt *ai = kmalloc(sizeof(struct acpi_interrupt));
    if(!ai)
        return UACPI_STATUS_OUT_OF_MEMORY;

    ai->ctx = ctx;
    ai->handler = handler;

    struct isr *isr = interrupt_allocate(acpi_irq, apic_send_eoi, IPL_ACPI);
    assert(isr);

    isr->userp = (void*) ai;
    io_apic_register_interrupt(irq, isr->id & 0xff, _cpu()->id, false);

    *out_irq_handle = isr;
    return UACPI_STATUS_OK;
}

uacpi_status uacpi_kernel_uninstall_interrupt_handler(
    uacpi_interrupt_handler, uacpi_handle irq_handle
) {
    struct isr *isr = irq_handle;
    if(!isr)
        return UACPI_STATUS_OK;

    struct acpi_interrupt *ai = isr->userp;
    kfree(ai);

    klog(ERROR, "io_apic_unregister not implemented!");

    interrupt_unregister(isr->id);
    return UACPI_STATUS_OK;
}

uacpi_handle uacpi_kernel_create_spinlock(void) {
    spinlock_t *lock = kmalloc(sizeof(spinlock_t));
    assert(lock != nullptr);

    spinlock_init(*lock); 
    return (uacpi_handle) lock;
}

void uacpi_kernel_free_spinlock(uacpi_handle handle) {
    spinlock_t *lock = (spinlock_t *) handle;

    kfree((void*) lock);
}

uacpi_cpu_flags uacpi_kernel_lock_spinlock(uacpi_handle handle) {
    assert(handle != nullptr);
    spinlock_t *lock = (spinlock_t *) handle;
    spinlock_acquire(lock);

    return UACPI_STATUS_OK;
}

void uacpi_kernel_unlock_spinlock(uacpi_handle handle, uacpi_cpu_flags flags) {
    assert(handle != nullptr);
    (void) flags;

    spinlock_t *lock = (spinlock_t *) handle;
    spinlock_release(lock); 
}

uacpi_status uacpi_kernel_schedule_work(
    uacpi_work_type, uacpi_work_handler, uacpi_handle
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
