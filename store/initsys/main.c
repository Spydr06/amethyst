#include <unistd.h>
#include <stdio.h>
#include <stddef.h>
#include <sys/mount.h>
#include <sys/stat.h>
#include <string.h>
#include <stdlib.h>

struct target {
    const char* mount_point;
    const char* fs;
};

struct target mount_targets[] = {
    {"/dev", "devfs"},
    {"/tmp", "tmpfs"}
};

void mount_target(struct target* target) {
    int err = mkdir(target->mount_point, 0666);
    if(err) {
        fprintf(stderr, "mkdir: faild creating '%s': %m\n", target->mount_point);
        exit(1);
    }

    err = mount(NULL, target->mount_point, target->fs, 0, NULL);
    if(err) {
        fprintf(stderr, "%s: mount(5) failed: %m\n", target->fs);
        exit(1);
    }
}

void umount_target(struct target* target) {
    int err = umount(target->mount_point);
    if(err) {
        fprintf(stderr, "%s: umount(1) failed: %m\n", target->fs);
        exit(1);
    }
}

int main(int argc, char** argv) {
    printf("Hello, World <3\n");

    for(size_t i = 0; i < sizeof(mount_targets) / sizeof(struct target); i++)
        mount_target(mount_targets + i);
    
    int fb = open("/dev/fb0", O_WRONLY, 0);
    if(!fb) {
        fprintf(stderr, "/dev/fb0: open() failed: %m\n");
        return 1;
    }

    close(fb);

    for(size_t i = 0; i < sizeof(mount_targets) / sizeof(struct target); i++)
        umount_target(mount_targets + i);

    while(1);
}

