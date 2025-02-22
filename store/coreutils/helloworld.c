#include <stdio.h>
#include <stdlib.h>

int main(int argc, char** argv) {
    puts("Hello, World <3\n");
    
    char* foo = malloc(1024);
    free(foo);

    return 0;
}

