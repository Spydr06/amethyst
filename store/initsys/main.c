#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <sys/mman.h>

void raw_puts(const char* msg) {
    write(1, msg, strlen(msg));
}

void test_brk() {
    void* orig_brk = sbrk(0);
    void* buf = sbrk(1024);
    if(buf == (void*) -1) {
        raw_puts("brk failed.\n");
        return;
    }

    memset(buf, 0xff, 1024);

    sbrk(-1024);
}

int main(int argc, char** argv) {
    raw_puts("Hello, World!\n");

    int fd = open("/include/unistd.h", 0, 0);
    if(fd < 0)
        return errno;

    size_t bufsize = 1024;
    char* buf = mmap(NULL, bufsize, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if(!buf || buf == MAP_FAILED) {
        raw_puts("mmap failed.\n");
        return 2;
    }

    int c = read(fd, buf, bufsize);

    close(fd);

    write(1, buf, c);

    munmap(buf, bufsize);

    test_brk();

    while(1);
}

