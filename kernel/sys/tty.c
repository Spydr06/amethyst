#include <sys/tty.h>
#include <sys/mutex.h>

#include <filesystem/device.h>
#include <mem/heap.h>

#include <assert.h>
#include <errno.h>

#define TTY_MAX_COUNT 4096
#define CONTROLLING_TTY_MINOR TTY_MAX_COUNT

mutex_t list_mutex;
//static struct tty* tty_list[TTY_MAX_COUNT];

static struct devops devops = {

};

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
    // TODO: minor

    tty->termios.c_iflag = ICRNL;
    tty->termios.c_oflag = ONLCR;
    tty->termios.c_lflag = ECHO | ICANON | ISIG | ECHOCTL;
    tty->termios.c_cflag = 0;
    tty->termios.c_cc[VINTR] = 0x03;
    tty->termios.c_ispeed = 38400;
    tty->termios.c_ospeed = 38400;
    tty->termios.c_cc[VMIN] = 1;

    tty->write_to_device = write_fn;
    tty->inactive_device = inactive_fn;
//    tty->minor = minor;
    tty->device_internal = internal;

//    struct vnode* new_node;
//    assert(devfs_getnode(nullptr, DEV_MAJOR_TTY, minor, &new_node) == 0);

    return tty;

error:
    if(tty->name)
        kfree(tty->name);

    if(tty->device_buffer)
        kfree(tty->device_buffer);

    kfree(tty);
    return nullptr;
}

void tty_init(void) {
    assert(devfs_register(&devops, "tty", V_TYPE_CHDEV, DEV_MAJOR_TTY, CONTROLLING_TTY_MINOR, 0644) == 0);
    mutex_init(&list_mutex);
}
