#ifndef _AMETHYST_SYS_TTY_H
#define _AMETHYST_SYS_TTY_H

#include <cdefs.h>
#include <stddef.h>

#include <io/poll.h>
#include <ringbuffer.h>
#include <sys/mutex.h>
#include <sys/termios.h>

#define TTY_DEVICE_BUFFER_SIZE 512
#define TTY_READ_BUFFER_SIZE PAGE_SIZE

typedef size_t (*ttydev_write_fn_t)(void* internal, const char* str, size_t size);
typedef void (*tty_inactive_fn_t)(void* internal);

struct tty {
    char* name;
    
    struct vnode* master_vnode;
    int minor;

    struct termios termios;

    struct io_poll io_poll;

    mutex_t read_mutex;
    ringbuffer_t read_buffer;

    char* line_buffer;
    size_t line_pos;

    ttydev_write_fn_t write_to_device;
    tty_inactive_fn_t inactive_device;

    struct winsize winsize;
    void* device_internal;
};

void tty_init(void);
struct tty* tty_create(const char* name, ttydev_write_fn_t write_fn, tty_inactive_fn_t inactive_fn, void* internal);
void tty_destroy(struct tty* tty);

void tty_process(struct tty* tty, char c);

void early_putchar(int ch);
void early_console_init(void);

#endif /* _AMETHYST_SYS_TTY_H */

