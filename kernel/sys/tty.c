#include "filesystem/virtual.h"
#include "io/poll.h"
#include "kernelio.h"
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
static int read(int minor, void* buffer, size_t size, uintmax_t offset, int flags, size_t* bytes_read);
static int write(int minor, void* _buffer, size_t size, uintmax_t offset, int flags, size_t* bytes_written);

static struct devops devops = {
    .write = write,
    .read = read,
    .inactive = inactive
};

static int register_minor(struct tty* tty) {
    mutex_acquire(&list_mutex);

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
    
    mutex_acquire(&list_mutex);

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

static int read(int minor, void* buffer, size_t size, uintmax_t offset, int flags, size_t* bytes_read) {
    (void) offset;
    (void) flags;

    struct tty* tty = tty_get(minor);
    if(!tty)
        return ENODEV;

    *bytes_read = 0;
    int min = (tty->termios.c_lflag & ICANON) ? 1 : tty->termios.c_cc[VMIN];
    int time = (tty->termios.c_lflag & ICANON) ? 0 : tty->termios.c_cc[VTIME];

    if(min == 0 && time == 0) {
        mutex_acquire(&tty->read_mutex);
        *bytes_read = ringbuffer_read(&tty->read_buffer, buffer, size);
        mutex_release(&tty->read_mutex);
        return 0;
    }

    // TODO: implement timeouts

    mutex_acquire(&tty->read_mutex);
    struct io_poll_queue* poll = io_poll_make(IO_POLL_IN);
    io_poll_add(&tty->io_poll, poll);
    mutex_release(&tty->read_mutex);

    int err = io_poll_wait(poll);
    
    mutex_acquire(&tty->read_mutex);
    *bytes_read += ringbuffer_read(&tty->read_buffer, buffer + *bytes_read, size - *bytes_read);
    mutex_release(&tty->read_mutex);

    io_poll_remove(&tty->io_poll, poll);
    io_poll_free_queue(poll);

    return err;
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

    char echo = tty->termios.c_lflag & ECHO ? c : '\0';

    // echo control chars
    if((tty->termios.c_lflag & ECHOCTL) && (tty->termios.c_lflag & ECHO) && isctrl(c)) {
        char tmp[2] = {'^', c + 0x40};
        tty->write_to_device(tty->device_internal, tmp, 2);
        echo = '\0';
    }

    // TODO: ISIG

    // line-based (canonical) mode, flush on `\n`, EOF, etc.
    if(tty->termios.c_lflag & ICANON) {
        if(c == tty->termios.c_cc[VERASE]) {
            // backspace
            if(tty->line_pos == 0)
                return;

            tty->line_buffer[--tty->line_pos] = '\0';

            if(tty->termios.c_lflag & ECHO)
                tty->write_to_device(tty->device_internal, "\b \b", 3);
            return;
        }
        
        bool flush = c == tty->termios.c_cc[VEOF] || 
                c == '\n' || 
                c == tty->termios.c_cc[VEOL] || 
                c == tty->termios.c_cc[VEOL2];

        if(echo)
            tty->write_to_device(tty->device_internal, &echo, 1);
        
        tty->line_buffer[tty->line_pos++] = c;
        if(tty->line_pos == TTY_DEVICE_BUFFER_SIZE)
            flush = true;

        if(flush) {
            mutex_acquire(&tty->read_mutex);
            ringbuffer_write(&tty->read_buffer, tty->line_buffer, tty->line_pos);
            mutex_release(&tty->read_mutex);

            memset(tty->line_buffer, 0, TTY_DEVICE_BUFFER_SIZE);
            tty->line_pos = 0;

            io_poll_event(&tty->io_poll, IO_POLL_IN);
        }

        return;
    }

    if(echo)
        tty->write_to_device(tty->device_internal, &c, 1);

    mutex_acquire(&tty->read_mutex);
    ringbuffer_write(&tty->read_buffer, &c, 1);
    mutex_release(&tty->read_mutex);

    io_poll_event(&tty->io_poll, IO_POLL_IN);
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

    tty->line_pos = 0;
    tty->line_buffer = kmalloc(TTY_DEVICE_BUFFER_SIZE);
    if(!tty->line_buffer)
        goto error;

    io_poll_init(&tty->io_poll);

    mutex_init(&tty->read_mutex);
    if(ringbuffer_init(&tty->read_buffer, TTY_READ_BUFFER_SIZE))
        goto error;

    int minor = register_minor(tty);
    if(minor == TTY_MAX_COUNT) {
        ringbuffer_destroy(&tty->read_buffer);
        goto error;
    }

    if(devfs_register(&devops, name, V_TYPE_CHDEV, DEV_MAJOR_TTY, minor, 0644)) {
        remove_minor(minor);
        ringbuffer_destroy(&tty->read_buffer);
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

    if(tty->line_buffer)
        kfree(tty->line_buffer);

    kfree(tty);
    return nullptr;
}

void tty_destroy(struct tty* tty) {
    remove_minor(tty->minor);
    ringbuffer_destroy(&tty->read_buffer);
    kfree(tty->line_buffer);
    kfree(tty->name);
    kfree(tty);
}

void tty_init(void) {
    assert(devfs_register(&devops, "tty", V_TYPE_CHDEV, DEV_MAJOR_TTY, CONTROLLING_TTY_MINOR, 0644) == 0);
    mutex_init(&list_mutex);
}
