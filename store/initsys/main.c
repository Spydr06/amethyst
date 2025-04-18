#include <errno.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mount.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <time.h>
#include <unistd.h>
#include <dirent.h>

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

    DIR* dir = opendir("/");
    if(!dir) {
        fprintf(stderr, "/: opendir() failed: %m\n");
        return 1;
    }

    struct dirent* ent;

    int i = 0;
    while((ent = readdir(dir))) {
        printf(" %d) \"%s\"\n", i++, ent->d_name);
    }

    closedir(dir);

next:
    int err = execv(shell_bin, (char* const[]){shell_bin, NULL});

    fprintf(stderr, "%s: error executing %s: %s\n", argv[0], shell_bin, strerror(errno));

    int pid = fork();
    if(pid == 0) {
        struct timespec ts = {.tv_nsec = 10000000, .tv_sec = 0};
        nanosleep(&ts, NULL);
        printf("fork you!\n");

        ts.tv_nsec = 0;
        ts.tv_sec = 1;
        nanosleep(&ts, NULL);
        printf("goodbye\n");

        while(1);
//        exit(1);
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

