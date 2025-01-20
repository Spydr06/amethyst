#include <unistd.h>
#include <stdio.h>

int main(int argc, char** argv) {
    FILE* fp = fopen("/include/unistd.h", "r");
    if(!fp) {
        fputs("fopen() failed.\n", stderr);
        return 1;
    }

    puts("Hello, World!\n");

    fclose(fp);

    while(1);
}

