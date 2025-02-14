#include "filesystem/virtual.h"
#include <sys/fb.h>

#include <stddef.h>
#include <string.h>
#include <errno.h>
#include <assert.h>

#include <limine/limine.h>
#include <mem/heap.h>
#include <mem/user.h>

#include <filesystem/device.h>
#include <kernelio.h>
#include <drivers/video/vga.h>

static size_t fb_count;
static struct fb_fix_screeninfo* fix_infos;
static struct fb_var_screeninfo* var_infos;

static int fb_read(int minor, void* buffer, size_t size, uintmax_t offset, int flags, size_t *readc);
static int fb_write(int minor, void* buffer, size_t size, uintmax_t offset, int flags, size_t *writec);
static int fb_maxseek(int minor, size_t* max);
static int fb_ioctl(int minor, uint64_t request, void* arg, int* result);
static int fb_mmap(int minor, void* addr, uintmax_t offset, int flags);

static struct devops fb_ops = {
    .read = fb_read,
    .write = fb_write,
    .maxseek = fb_maxseek,
    .ioctl = fb_ioctl,
    .mmap = fb_mmap,
};

static inline const struct limine_framebuffer* fb_get(int minor) {
    if(minor >= 0 && (size_t) minor < limine_fb_request.response->framebuffer_count)
        return limine_fb_request.response->framebuffers[minor];
    return nullptr;
}

static int fb_read(int minor, void* buffer, size_t size, uintmax_t offset, int flags __unused, size_t *readc) {
    const struct limine_framebuffer* fb = fb_get(minor);
    if(!fb)
        return ENODEV;

    uintmax_t end = fb->pitch * fb->height;
    *readc = 0;

    if(offset >= end)
        return 0;

    if(size + offset > end)
        size = end - offset;

    memcpy(buffer, fb->address + offset, size);

    *readc = size;
    return 0;
}

static int fb_write(int minor, void* buffer, size_t size, uintmax_t offset, int flags __unused, size_t *writec) {
    const struct limine_framebuffer* fb = fb_get(minor);
    if(!fb)
        return ENODEV;

    uintmax_t end = fb->pitch * fb->height;
    *writec = 0;

    if(size + offset > end)
        size = end - offset;

    memcpy(fb->address + offset, buffer, size);

    *writec = size;
    return 0;
}

static int fb_maxseek(int minor, size_t* max) {
    const struct limine_framebuffer* fb = fb_get(minor);
    if(!fb)
        return ENODEV;

    *max = fb->pitch * fb->height;
    return 0;
}

static int fb_mmap(int minor, void* addr, uintmax_t offset, int flags) {
    const struct limine_framebuffer* fb = fb_get(minor);
    if(!fb)
        return ENODEV;

    uintmax_t end = fb->pitch * fb->height;
    assert(offset < end);

    if(flags & V_FFLAGS_SHARED) {
        assert((offset % PAGE_SIZE) == 0);
        return mmu_map(_cpu()->thread->vmm_context->page_table, FROM_HHDM((void*)((uintptr_t) fb->address + offset)), addr, vnode_to_mmu_flags(flags)) ? 0 : ENOMEM;
    }

    size_t size = offset + PAGE_SIZE < end ? PAGE_SIZE : end - offset;
    void* paddr = pmm_alloc_page(PMM_SECTION_DEFAULT);
    if(!paddr)
        return ENOMEM;

    memcpy(MAKE_HHDM(paddr), (void*)((uintptr_t) fb->address + offset), size);

    if(!mmu_map(_cpu()->thread->vmm_context->page_table, paddr, addr, vnode_to_mmu_flags(flags))) {
        pmm_free_page(paddr);
        return ENOMEM;
    }

    return 0;
}

static int fb_ioctl(int minor, uint64_t request, void* arg, int* __unused) {
    if(minor < 0 || (size_t) minor >= fb_count)
        return ENODEV;

    switch(request) {
        case FBIOGET_FSCREENINFO:
            return memcpy_maybe_to_user(arg, fix_infos + minor, sizeof(struct fb_fix_screeninfo));
        case FBIOGET_VSCREENINFO:
            return memcpy_maybe_to_user(arg, var_infos + minor, sizeof(struct fb_var_screeninfo));
        default:
            return ENOTTY;
    }
}

void fbdev_init(void) {
    if(!limine_fb_request.response || !limine_fb_request.response->framebuffer_count) {
        klog(WARN, "No framebuffer devices found\n");
        return;
    }

    fb_count = limine_fb_request.response->framebuffer_count;
    fix_infos = kcalloc(fb_count, sizeof(struct fb_fix_screeninfo));
    var_infos = kcalloc(fb_count, sizeof(struct fb_var_screeninfo));

    char name[64];

    for(size_t i = 0; i < fb_count; i++) {
        struct limine_framebuffer* fb = limine_fb_request.response->framebuffers[i];

        memcpy(name, "fb", 3);
        utoa((uint64_t) i, name + 2, 10);

        struct fb_var_screeninfo* var = var_infos + i;
        var->xres = fb->width;
        var->yres = fb->height;
        var->bits_per_pixel = fb->bpp;
        var->red.msb_right = 1;
        var->red.offset = fb->red_mask_shift;
        var->red.length = fb->red_mask_size;
        var->green.msb_right = 1;
        var->green.offset = fb->green_mask_shift;
        var->green.length = fb->green_mask_size;
        var->blue.msb_right = 1;
        var->blue.offset = fb->blue_mask_shift;
        var->blue.length = fb->blue_mask_size;
        var->trans.msb_right = 1;

        struct fb_fix_screeninfo* fix = fix_infos + i;
        strncpy(fix->id, "LIMINE FB", __len(fix->id));
        fix->smem_start = fb->address;
        fix->smem_len = fb->height * fb->pitch;
        fix->type = FB_TYPE_PACKED_PIXELS;
        fix->visual = FB_VISUAL_TRUECOLOR;
        fix->line_length = fb->pitch;

        int err = devfs_register(&fb_ops, name, V_TYPE_CHDEV, DEV_MAJOR_FB, i, 0666);
        if(err) {
            klog(ERROR, "Failed registering framebuffer device '%s': %s", name, strerror(err));
            continue;
        }

        klog(DEBUG, "registered framebuffer device '%s' [%lux%lux%u].", name, fb->width, fb->height, fb->bpp);
    }
}
