#include <sys/tty.h>
#include <sys/mutex.h>

#include <filesystem/device.h>

#include <assert.h>

#define TTY_MAX_COUNT 4096
#define CONTROLLING_TTY_MINOR TTY_MAX_COUNT

mutex_t list_mutex;

static struct devops devops = {

};

void tty_init(void) {
    assert(devfs_register(&devops, "tty", V_TYPE_CHDEV, DEV_MAJOR_TTY, CONTROLLING_TTY_MINOR, 0644) == 0);
    mutex_init(&list_mutex);
}
