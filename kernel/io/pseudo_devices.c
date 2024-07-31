#include <io/pseudo_devices.h>

#include <filesystem/device.h>
#include <sys/timekeeper.h>

#include <cdefs.h>
#include <errno.h>
#include <kernelio.h>
#include <string.h>
#include <rand.h>

struct pseudo_dev {
    const char* name;
    int major;
    int minor;

    struct devops ops;
};

static int null_write(int __unused, void* __unused, size_t count, uintmax_t __unused, int __unused, size_t *bytes_written) {
    *bytes_written = count;
    return 0;
}

static int null_read(int __unused, void* __unused, size_t count, uintmax_t __unused, int __unused, size_t* bytes_read) {
    *bytes_read = count;
    return 0;
}

static int full_write(int __unused, void* __unused, size_t __unused, uintmax_t __unused, int __unused, size_t* __unused) {
    return ENOSPC;
}

static int zero_read(int __unused, void* buffer, size_t count, uintmax_t __unused, int __unused, size_t* bytes_read) {
    memset(buffer, 0, count);
    *bytes_read = count;
    return 0;
}

static int urandom_read(int __unused, void* buffer, size_t count, uintmax_t __unused, int __unused, size_t* bytes_read) {
    uint8_t* bytes = buffer;
    for(size_t i = 0; i < count; i++)
        bytes[i] = rand() & 0xff;

    *bytes_read = count;
    return 0;
}

static struct pseudo_dev devices[] = {
    {
        "null",
        DEV_MAJOR_NULL,
        0,
        {
            .write = null_write,
            .read = null_read
        }
    },
    {
        "full",
        DEV_MAJOR_FULL,
        0,
        {
            .write = full_write,
            .read = zero_read
        }
    },
    {
        "zero",
        DEV_MAJOR_ZERO,
        0,
        {
            .write = null_write,
            .read = zero_read
        }
    },
    {
        "urandom",
        DEV_MAJOR_URANDOM,
        0,
        {
            .write = null_write,
            .read = urandom_read
        }
    }
};

void pseudodevices_init(void) {
    srand(timekeeper_time_from_boot().ns);

    int err;
    for(size_t i = 0; i < __len(devices); i++) {
        struct pseudo_dev* dev = devices + i;
        if((err = devfs_register(&dev->ops, dev->name, V_TYPE_CHDEV, dev->major, dev->minor, 0644)))
            panic("failed installing chdev `%s`: %s", dev->name, strerror(err));
    }
}
