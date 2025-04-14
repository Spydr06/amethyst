#include <stdio.h>

extern void greet(const char** names, unsigned num) {
    printf("Hello, World to\n");
    for(unsigned i = 0; i < num; i++) {
        printf("  %s\n", names[i]); 
    }
}

