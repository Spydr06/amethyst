#include <unistd.h>
#include <stdio.h>
#include <stddef.h>
#include <sys/mount.h>
#include <sys/stat.h>
#include <string.h>
#include <stdlib.h>

int main(int argc, char** argv) {
    printf("Hello, World <3\n");

    int err = mkdir("/dev", 0);
    if(err) {
        fprintf(stderr, "mkdir: failed creating '/dev': %m\n");
        return 1;
    }
    
    err = mount(NULL, "/dev", "devfs", 0, NULL);
    if(err) {
        fprintf(stderr, "devfs: mount() failed: %m\n");
        return 1;
    }

    int fb = open("/dev/fb0", O_WRONLY, 0);
    if(!fb) {
        fprintf(stderr, "/dev/fb0: open() failed: %m\n");
        return 1;
    }

    close(fb);

    err = umount("/dev");
    if(err) {
        fprintf(stderr, "devfs: umount() failed: %m\n");
        return 1;
    }

    while(1);
}

