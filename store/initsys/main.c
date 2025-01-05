#include <unistd.h>
#include <string.h>
#include <errno.h>

int main(int argc, char** argv) {
    char* msg = "Hello, World!\n"; 
    write(1, msg, strlen(msg));

    int fd = open("/include/unistd.h", 0, 0);
    if(fd < 0)
        return errno;

    char buf[1024];
    int c = read(fd, buf, sizeof buf);

    close(fd);

    write(1, buf, c);

    while(1);
}

