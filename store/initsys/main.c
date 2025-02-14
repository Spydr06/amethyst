#include <unistd.h>
#include <stdio.h>
#include <stddef.h>
#include <sys/mount.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <string.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <amethyst/fb.h>

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

    struct fb_var_screeninfo fb_vi;
    ioctl(fb, FBIOGET_VSCREENINFO, &fb_vi);

    struct fb_fix_screeninfo fb_fi;
    ioctl(fb, FBIOGET_FSCREENINFO, &fb_fi);

    void* fb_buf = mmap(NULL, fb_fi.smem_len, PROT_WRITE, MAP_SHARED, fb, 0);
    if(!fb_buf) {
        fprintf(stderr, "/dev/fb0: mmap() failed: %m\n");
    }

    memset(fb_buf, 0xff, fb_fi.smem_len);
    printf("Here!\n");

    close(fb);

    for(size_t i = 0; i < sizeof(mount_targets) / sizeof(struct target); i++)
        umount_target(mount_targets + i);

    while(1);
}

