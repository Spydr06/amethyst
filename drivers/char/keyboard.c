#include "filesystem/virtual.h"
#include "x86_64/cpu/idt.h"
#include <drivers/char/keyboard.h>

#include <filesystem/device.h>
#include <hashtable.h>
#include <ringbuffer.h>
#include <sys/mutex.h>

#include <assert.h>
#include <errno.h>
#include <string.h>

#define BUFFER_SIZE (100 * sizeof(struct keyboard_event))

static int kb_read(int minor, void* buffer, size_t size, uintmax_t offset, int flags, size_t* readc);

static struct keyboard keyboard_console;

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
    mutex_acquire(&table_lock, false);

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

    }

    event.flags |= kb->flags;

    spinlock_acquire(&kb->lock);
    assert(ringbuffer_write(&kb->event_buffer, &event, sizeof(struct keyboard_event)) != 0);
    spinlock_release(&kb->lock);

    spinlock_acquire(&keyboard_console.lock);
    assert(ringbuffer_write(&keyboard_console.event_buffer, &event, sizeof(struct keyboard_event)) != 0);
    spinlock_release(&keyboard_console.lock);

    interrupt_lower_ipl(ipl);
}

static struct keyboard* keyboard_get(int minor) {
    struct keyboard* kb;
    mutex_acquire(&table_lock, false);
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

static int keyboard_wait(struct keyboard* kb, struct keyboard_event* event) {
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
