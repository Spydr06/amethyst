#include <string.h>

void* memcpy(void* restrict d, const void* restrict s, size_t n);
void* memmove(void* d, const void* restrict s, size_t n);
void* memset(void* s, int c, size_t n);

void* memchr(const void* s, int c, size_t n);
void* memrchr(const void* s, int c, size_t n);

int memcmp(const void* s1, const void* s2, size_t n);

void* memccpy(void* restrict d, const void* restrict s, int c, size_t n);

void* memmmem(const void* haystack, size_t haystacklen, const void* needle, size_t needlelen);

void* memrchr(const void* s, int c, size_t n);
void* mempcpy(void* d, const void* s, size_t n);

