#include <drivers/char/keyboard.h>

#include <filesystem/devfs.h>
#include <filesystem/vfs.h>
#include <sys/mutex.h>

#include <hashtable.h>
#include <ringbuffer.h>

#include <assert.h>
#include <ctype.h>
#include <errno.h>
#include <string.h>

#define BUFFER_SIZE (100 * sizeof(struct keyboard_event))

static int kb_read(int minor, void* buffer, size_t size, uintmax_t offset, int flags, size_t* readc);

struct keyboard keyboard_console;

static hashtable_t keyboard_table;
static mutex_t table_lock;

static int current_keyboard_num;

static struct devops devops = {
    .read = kb_read,
};

void keyboard_driver_init(void) {
    mutex_init(&table_lock);
    assert(keyboard_init(&keyboard_console) == 0);
    assert(hashtable_init(&keyboard_table, MAX_KEYBOARDS) == 0);
}

int keyboard_init(struct keyboard* kb) {
    memset(kb, 0, sizeof(struct keyboard));

    int err = ringbuffer_init(&kb->event_buffer, BUFFER_SIZE);
    if(err)
        return err;

    semaphore_init(&kb->semaphore, 0);
    return 0;
}

void keyboard_deinit(struct keyboard* kb) {
    ringbuffer_destroy(&kb->event_buffer);
}

int keyboard_register(struct keyboard* kb) {
    mutex_acquire(&table_lock);

    int err = hashtable_set(&keyboard_table, kb, &current_keyboard_num, sizeof(current_keyboard_num), true);
    if(err) {
        mutex_release(&table_lock);
        return err;
    }

    mutex_release(&table_lock);

    char name[32] = "kb";
    itoa(current_keyboard_num, name + 2, 10);

    assert(devfs_register(&devops, name, V_TYPE_CHDEV, DEV_MAJOR_KEYBOARD, current_keyboard_num, 0666) == 0);
    current_keyboard_num++;

    return 0;
}

void keyboard_event(struct keyboard* kb, struct keyboard_event event) {
    enum ipl ipl = interrupt_raise_ipl(IPL_KEYBOARD);

    switch(event.keycode) {
        case KEYCODE_LCTRL:
            if (event.flags & KEYBOARD_EVENT_RELEASED)
                kb->flags &= ~KEYBOARD_EVENT_LCTRL;
            else
                kb->flags |= KEYBOARD_EVENT_LCTRL;
            break;
        case KEYCODE_RCTRL:
            if (event.flags & KEYBOARD_EVENT_RELEASED)
                kb->flags &= ~KEYBOARD_EVENT_RCTRL;
            else
                kb->flags |= KEYBOARD_EVENT_RCTRL;
            break;
        case KEYCODE_CAPSLOCK:
            if (event.flags & KEYBOARD_EVENT_RELEASED)
                kb->flags &= ~KEYBOARD_EVENT_CAPSLOCK;
            else
                kb->flags |= KEYBOARD_EVENT_CAPSLOCK;
            break;
        case KEYCODE_LALT:
            if (event.flags & KEYBOARD_EVENT_RELEASED)
                kb->flags &= ~KEYBOARD_EVENT_LALT;
            else
                kb->flags |= KEYBOARD_EVENT_LALT;
            break;
        case KEYCODE_RALT:
            if (event.flags & KEYBOARD_EVENT_RELEASED)
                kb->flags &= ~KEYBOARD_EVENT_RALT;
            else
                kb->flags |= KEYBOARD_EVENT_RALT;
            break;
        case KEYCODE_LSHIFT:
            if (event.flags & KEYBOARD_EVENT_RELEASED)
                kb->flags &= ~KEYBOARD_EVENT_LSHIFT;
            else
                kb->flags |= KEYBOARD_EVENT_LSHIFT;
            break;
        case KEYCODE_RSHIFT:
            if (event.flags & KEYBOARD_EVENT_RELEASED)
                kb->flags &= ~KEYBOARD_EVENT_RSHIFT;
            else
                kb->flags |= KEYBOARD_EVENT_RSHIFT;
            break;
	}

    event.flags |= kb->flags;

    spinlock_acquire(&kb->lock);
    if(ringbuffer_write(&kb->event_buffer, &event, sizeof(struct keyboard_event)) != 0)
        semaphore_signal(&kb->semaphore);
    spinlock_release(&kb->lock);

    spinlock_acquire(&keyboard_console.lock);
    if(ringbuffer_write(&keyboard_console.event_buffer, &event, sizeof(struct keyboard_event)) != 0)
        semaphore_signal(&keyboard_console.semaphore);
    spinlock_release(&keyboard_console.lock);

    interrupt_lower_ipl(ipl);
}

static struct keyboard* keyboard_get(int minor) {
    struct keyboard* kb;
    mutex_acquire(&table_lock);
    hashtable_get(&keyboard_table, (void**) &kb, &minor, sizeof(int));
    mutex_release(&table_lock);
    return kb;
}

static bool keyboard_fetch_event(struct keyboard* kb, struct keyboard_event* event) {
    if(!semaphore_test(&kb->semaphore))
        return false;

    enum ipl ipl = interrupt_raise_ipl(IPL_KEYBOARD);

    spinlock_acquire(&kb->lock);
    bool ok = ringbuffer_read(&kb->event_buffer, event, sizeof(struct keyboard_event)) == sizeof(struct keyboard_event);
    spinlock_release(&kb->lock);

    interrupt_lower_ipl(ipl);
    return ok;
}

int keyboard_wait(struct keyboard* kb, struct keyboard_event* event) {
    int err = semaphore_wait(&kb->semaphore, true);
    if(err)
        return err;

    enum ipl ipl = interrupt_raise_ipl(IPL_KEYBOARD);

    spinlock_acquire(&kb->lock);
    assert(ringbuffer_read(&kb->event_buffer, event, sizeof(struct keyboard_event)) == sizeof(struct keyboard_event));
    spinlock_release(&kb->lock);

    interrupt_lower_ipl(ipl);
    return 0;
}

static int kb_read(int minor, void* buffer, size_t size, uintmax_t __unused, int flags, size_t* readc) {
    struct keyboard* kb = keyboard_get(minor);
    if(!kb)
        return ENODEV;
    
    *readc = 0;
    size_t count = size / sizeof(struct keyboard_event);
    if(count == 0)
        return 0;

    for(size_t i = 0; i < count; i++) {
        struct keyboard_event event;
        if(!keyboard_fetch_event(kb, &event)) {
            if(i != 0 || flags & V_FFLAGS_NONBLOCKING)
                break;

            int err = keyboard_wait(kb, &event);
            if(err)
                return err;
        }
        
        memcpy(buffer + *readc, &event, sizeof(struct keyboard_event));
        *readc += sizeof(struct keyboard_event);
    }

    return 0;
}

static char table_lowercase[128] = {
    0, '\033', '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '=', '\b', '\t',
    'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '[', ']', '\r', 0,
    'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', ';', '\'', '`', 0, '\\',
    'z', 'x', 'c', 'v', 'b', 'n', 'm', ',', '.', '/', 0, '*', 0, ' ', 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
    '7', '8', '9', '-', '4', '5', '6', '+', '1', '2', '3', '.', 0, 0, '\b', 0, '/', 0, 0, '\r', 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
};

static char table_uppercase[128] = {
    0, '\033', '!', '@', '#', '$', '%', '^', '&', '*', '(', ')', '_', '+', '\b', '\t',
    'Q', 'W', 'E', 'R', 'T', 'Y', 'U', 'I', 'O', 'P', '{', '}', '\r', 0,
    'A', 'S', 'D', 'F', 'G', 'H', 'J', 'K', 'L', ':', '"', '~', 0, '|',
    'Z', 'X', 'C', 'V', 'B', 'N', 'M', '<', '>', '?', 0, '*', 0, ' ', 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
    '7', '8', '9', '-', '4', '5', '6', '+', '1', '2', '3', '.', 0, 0, '\b', 0, '/', 0, 0, '\r', 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
};

char keyboard_event_as_ascii(struct keyboard_event event) {
    assert(event.keycode < sizeof(table_lowercase));

    if(event.flags & KEYBOARD_EVENT_CAPSLOCK) {
        if(event.flags & (KEYBOARD_EVENT_LSHIFT | KEYBOARD_EVENT_RSHIFT))
            return tolower(table_uppercase[event.keycode]);
        return toupper(table_lowercase[event.keycode]);
    }
    else {
        if(event.flags & (KEYBOARD_EVENT_LSHIFT | KEYBOARD_EVENT_RSHIFT))
            return table_uppercase[event.keycode];
        return table_lowercase[event.keycode];
    }
}

