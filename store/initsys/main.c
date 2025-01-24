#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(int argc, char** argv) {
    printf("Hello, World <3\n");

    FILE* f = fopen("/include/unistd.h", "r");
    if(!f) {
        fprintf(stderr, "fopen() failed: %m\n");
        return 1;
    }

    char* buf = malloc(1024);
    size_t r = fread(buf, 1023, sizeof(char), f);

    fwrite(buf, r, sizeof(char), stdout);

    free(buf);
    fclose(f);

    while(1);
}

