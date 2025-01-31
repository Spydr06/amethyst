#include <sys/fb.h>

#include <stddef.h>
#include <string.h>
#include <errno.h>

#include <limine/limine.h>

#include <filesystem/device.h>
#include <kernelio.h>
#include <drivers/video/vga.h>

static int fb_read(int minor, void* buffer, size_t size, uintmax_t offset, int flags, size_t *readc);
static int fb_write(int minor, void* buffer, size_t size, uintmax_t offset, int flags, size_t *writec);
static int fb_maxseek(int minor, size_t* max);

static struct devops fb_ops = {
    .read = fb_read,
    .write = fb_write,
    .maxseek = fb_maxseek,
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

void fbdev_init(void) {
    if(!limine_fb_request.response || !limine_fb_request.response->framebuffer_count) {
        klog(WARN, "No framebuffer devices found\n");
        return;
    }

    char name[64];

    for(size_t i = 0; i < limine_fb_request.response->framebuffer_count; i++) {
        struct limine_framebuffer* fb = limine_fb_request.response->framebuffers[i];

        memcpy(name, "fb", 3);
        utoa((uint64_t) i, name + 2, 10);

        int err = devfs_register(&fb_ops, name, V_TYPE_CHDEV, DEV_MAJOR_FB, i, 0666);
        if(err) {
            klog(ERROR, "Failed registering framebuffer device '%s': %s", name, strerror(err));
            continue;
        }

        klog(DEBUG, "registered framebuffer device '%s' [%lux%lux%u].", name, fb->width, fb->height, fb->bpp);
    }
}
