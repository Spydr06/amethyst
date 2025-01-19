#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <stdlib.h>

void raw_puts(const char* msg) {
    write(1, msg, strlen(msg));
}


int main(int argc, char** argv) {
    raw_puts("Hello, World!\n");

    int fd = open("/include/unistd.h", 0, 0);
    if(fd < 0)
        return errno;

    size_t bufsize = 1024;
    void* buf = malloc(bufsize);
    if(!buf) {
        raw_puts("malloc failed.\n");
        return 2;
    }

    int c = read(fd, buf, bufsize);

    close(fd);

    write(1, buf, c);

    free(buf);

    while(1);
}

