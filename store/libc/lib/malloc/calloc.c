#include <stdlib.h>

#include <errno.h>
#include <string.h>

void *calloc(size_t nmemb, size_t size) {
    if(size && nmemb > (size_t) -1 / size) {
        errno = ENOMEM;
        return NULL;
    }

    nmemb *= size;
    void *ptr = malloc(nmemb);
    if(ptr)
        memset(ptr, 0, nmemb);

    return ptr;
}
