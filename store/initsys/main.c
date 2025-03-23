#include <unistd.h>
#include <stdio.h>
#include <stddef.h>
#include <sys/mount.h>
#include <sys/stat.h>
#include <string.h>
#include <stdlib.h>
#include <sys/syscall.h>
#include <errno.h>

static char shell_bin[] = "/bin/shard-sh";

int draw_logo_to_fb(int fb, int scale);

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

    draw_logo_to_fb(fb, 6);

    close(fb);

    int err = execv(shell_bin, (char* const[]){shell_bin, NULL});

    fprintf(stderr, "%s: error executing %s: %s\n", argv[0], shell_bin, strerror(errno));

    int pid = fork();
    if(pid == 0) {
        printf("fork you!\n");
        exit(1);
    }

    printf("forked pid %d\n", pid);

    char buffer[100];
    while(1) {
        printf("echo> ");
        fflush(stdout);
        fgets(buffer, sizeof(buffer), stdin);
        puts(buffer);
    }

    for(size_t i = 0; i < sizeof(mount_targets) / sizeof(struct target); i++)
        umount_target(mount_targets + i);

    fflush(stderr);
}

