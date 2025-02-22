#include <stdio.h>

extern void foo();
int main(int argc, char** argv) {
    puts("Hello, World <3\n");
    foo();
    return 0;
}

