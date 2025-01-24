#include <unistd.h>
#include <stdio.h>

int main(int argc, char** argv) {
    printf("Hello, World <3 %d, %p, %s\n", 69, (void*) main, "foobar!");

    while(1);
}

