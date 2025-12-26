#include "initsys.h"

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
#include <getopt.h>
#include <sys/time.h>

#include <libshard.h>
#include <shard_libc_driver.h>

#define DEFAULT_CONFIGURATION_PATH "/bin/configuration.shard"

static char shell_bin[] = "/bin/shard";

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

static struct option long_options[] = {
    { "loglevel",    required_argument, NULL, 'l' },
    { "module-list", required_argument, NULL, 'm'},
    { "help",        no_argument,       NULL, 'h' },
    { NULL, 0, NULL, 0 },
};

static void usage(FILE* outf) {
    fprintf(outf, "Usage: %s [options] <configuration file>\n"
                  "       %s [-h | --help]\n", _getprogname(), _getprogname());
}

static void help(void) {
    usage(stdout); 
    
    printf("Options:\n"
        "-m, --module-list <path>   Set the module list path\n"
        "-l, --loglevel <num>   Set the log level to <num>\n"
        "-h, --help             Display this help text and exit\n"
    );
}

int main(int argc, char** argv) {
    int c;

    const char *module_list_path = DEFAULT_MODULE_LIST;
    while((c = getopt_long(argc, argv, "l:h", long_options, NULL)) != EOF) {
        switch(c) {
            case 'h':
                help();
            case 'm':
                module_list_path = optarg;
            case '?':
                break;
            default:
                usage(stderr);
                return EXIT_FAILURE;
        }
    }

    if(optind < argc - 1) {
        fprintf(stderr, "%s: excessive arguments\n", argv[0]);
        usage(stderr);
        return EXIT_FAILURE;
    }
 
    const char* configpath = optind < argc ? argv[optind] : DEFAULT_CONFIGURATION_PATH;

    pid_t pid = getpid();
    if(pid != 1) {
        fprintf(stderr, "%s: not running as pid 1 [%d]", argv[0], pid);
        return EXIT_FAILURE;
    }

    printf("Hello, World <3\n");

    for(size_t i = 0; i < sizeof(mount_targets) / sizeof(struct target); i++)
        mount_target(mount_targets + i);
    
    int fb = open("/dev/fb0", O_WRONLY, 0);
    if(!fb) {
        fprintf(stderr, "/dev/fb0: open() failed: %m\n");
        return EXIT_FAILURE;
    }

    draw_logo_to_fb(fb, 6);

    close(fb);

    struct shard_context ctx;
    shard_context_default(&ctx);

    int err = shard_init(&ctx);
    if(err) {
        fprintf(stderr, "%s: error initializing libshard: %s\n", argv[0], strerror(err));
        return EXIT_FAILURE;
    }

    // load modules:
    if((err = load_kernel_modules(&ctx, module_list_path))) {
        fprintf(stderr, "%s: failed loading modules specified in `%s`: %s\n", argv[0], module_list_path, strerror(err));
        fflush(stderr);
    }

    shard_deinit(&ctx);

    execv(shell_bin, (char* const[]){shell_bin, NULL});
    fprintf(stderr, "%s: error executing %s: %s\n", argv[0], shell_bin, strerror(errno));

    for(size_t i = 0; i < sizeof(mount_targets) / sizeof(struct target); i++)
        umount_target(mount_targets + i);

    return EXIT_FAILURE;
}

