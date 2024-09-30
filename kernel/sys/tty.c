#include "filesystem/virtual.h"
#include "sys/termios.h"
#include <sys/tty.h>
#include <sys/mutex.h>

#include <filesystem/device.h>
#include <mem/heap.h>

#include <assert.h>
#include <errno.h>
#include <ctype.h>

#define TTY_MAX_COUNT 4096
#define CONTROLLING_TTY_MINOR TTY_MAX_COUNT

mutex_t list_mutex;
static struct tty* tty_list[TTY_MAX_COUNT];

static void inactive(int minor); 
static int write(int minor, void* _buffer, size_t size, uintmax_t offset, int flags, size_t* bytes_written);

static struct devops devops = {
    .write = write,
    .inactive = inactive
};

static int register_minor(struct tty* tty) {
    mutex_acquire(&list_mutex, false);

    int i;
    for(i = 0; i < TTY_MAX_COUNT; i++) {
        if(!tty_list[i]) {
            tty_list[i] = tty;
            goto finish;
        }
    }

finish:
    mutex_release(&list_mutex);
    return i;
}

static void remove_minor(int minor) {
    if(minor >= TTY_MAX_COUNT)
        return;
    
    mutex_acquire(&list_mutex, false);

    if(!tty_list[minor])
        klog(WARN, "tty list entry `%d` is already NULL.", minor);

    tty_list[minor] = nullptr;
    mutex_release(&list_mutex);
}

static inline struct tty* tty_get(int minor) {
    return minor < TTY_MAX_COUNT ? tty_list[minor] : nullptr;
}

static void inactive(int minor) {
    struct tty* tty = tty_get(minor);
    assert(tty);

    if(tty->inactive_device)
        tty->inactive_device(tty->device_internal);

    tty_destroy(tty);
} 

static int write(int minor, void* buffer, size_t size, uintmax_t __unused, int __unused, size_t* bytes_written) {
    struct tty* tty = tty_get(minor);
    if(!tty)
        return ENODEV;

    *bytes_written = size;
    for(size_t i = 0; i < size; i++) {
        if(((char*) buffer)[i] == '\n' && (tty->termios.c_oflag & ONLCR)) {
            char cr = '\r';
            tty->write_to_device(tty->device_internal, &cr, 1);
        }
        tty->write_to_device(tty->device_internal, buffer + i, 1);
    }

    return 0;
}
void tty_process(struct tty* tty, char c) {
    if(c == '\r' && (tty->termios.c_iflag & IGNCR))
        return;

    if(c == '\r' && (tty->termios.c_iflag & ICRNL))
        c = '\n';
    else if(c == '\n' && (tty->termios.c_iflag & INLCR))
        c = '\r';

    // TODO: VSTART, VSTOP

    char echo_char = tty->termios.c_lflag & ECHO ? c : '\0';

    // echo control chars
    if((tty->termios.c_lflag & ECHOCTL) && (tty->termios.c_lflag & ECHO) && isctrl(c)) {
        char tmp[2] = {'^', c + 0x40};
        tty->write_to_device(tty->device_internal, tmp, 2);
        echo_char = '\0';
    }

    // TODO: ISIG

    // TODO: buffers and other flags

    tty->write_to_device(tty->device_internal, &echo_char, 1);
}

struct tty* tty_create(const char* name, ttydev_write_fn_t write_fn, tty_inactive_fn_t inactive_fn, void* internal) {
    struct tty* tty = kmalloc(sizeof(struct tty));
    if(!tty) {
        errno = ENOMEM;
        return NULL;
    }

    tty->name = kmalloc(strlen(name) + 1);
    if(!tty->name)
        goto error;

    tty->device_buffer = kmalloc(TTY_DEVICE_BUFFER_SIZE);
    if(!tty->device_buffer)
        goto error;

    // TODO: ringbuffer
    int minor = register_minor(tty);
    if(minor == TTY_MAX_COUNT) {
        // TODO: destroy ringbuffer
        goto error;
    }

    if(devfs_register(&devops, name, V_TYPE_CHDEV, DEV_MAJOR_TTY, minor, 0644)) {
        remove_minor(minor);
        // TODO: destroy ringbuffer
        goto error;
    }

    tty->termios.c_iflag = ICRNL;
    tty->termios.c_oflag = ONLCR;
    tty->termios.c_lflag = ECHO | ICANON | ISIG | ECHOCTL;
    tty->termios.c_cflag = 0;

    tty->termios.c_ispeed = 38400;
    tty->termios.c_ospeed = 38400;

    tty->termios.c_cc[VMIN] = 1;
    tty->termios.c_cc[VINTR] = 0x03;
    tty->termios.c_cc[VQUIT] = 0x1c;
    tty->termios.c_cc[VERASE] = '\b';
    tty->termios.c_cc[VKILL] = 0x15;
    tty->termios.c_cc[VEOF] = 0x04;
    tty->termios.c_cc[VSTART] = 0x11;
    tty->termios.c_cc[VSTOP] = 0x13;
    tty->termios.c_cc[VSUSP] = 0x1a;

    tty->write_to_device = write_fn;
    tty->inactive_device = inactive_fn;
    tty->minor = minor;
    tty->device_internal = internal;

    struct vnode* new_node;
    assert(devfs_getnode(nullptr, DEV_MAJOR_TTY, minor, &new_node) == 0);

    tty->master_vnode = (struct vnode*)((struct dev_node*) new_node)->master;

    vop_release(&new_node);
    return tty;

error:
    if(tty->name)
        kfree(tty->name);

    if(tty->device_buffer)
        kfree(tty->device_buffer);

    kfree(tty);
    return nullptr;
}

void tty_destroy(struct tty* tty) {
    remove_minor(tty->minor);
    // TODO: destroy ringbuffer
    kfree(tty->device_buffer);
    kfree(tty->name);
    kfree(tty);
}

void tty_init(void) {
    assert(devfs_register(&devops, "tty", V_TYPE_CHDEV, DEV_MAJOR_TTY, CONTROLLING_TTY_MINOR, 0644) == 0);
    mutex_init(&list_mutex);
}
