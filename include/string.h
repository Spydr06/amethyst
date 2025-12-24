#ifndef _STRING_H
#define _STRING_H

#include <stddef.h>
#include <stdint.h>

char* reverse(char* str, size_t len);
char* utoa(uint64_t num, char* str, int base);
char* itoa(int64_t num, char* str, int base);

char* pretty_size_toa(size_t size, char* buf);

size_t strlen(const char* s);

#define _AMETHYST_STRNLEN_DEFINED 1

size_t strnlen(const char* s, size_t maxlen);
char* strcpy(char* dest, const char* src);
char* strncpy(char* dst, const char* restrict src, size_t dsize);
char* stpncpy(char *restrict dst, const char *restrict src, size_t dsize);

char* strncat(char* restrict d, const char* restrict s, size_t n);

int strncmp(const char* _l, const char* _r, size_t n);
int strcmp(const char* _l, const char* _r);
int strcasecmp(const char* _l, const char *_r);

size_t strspn(const char* s, const char* accept);
size_t strcspn(const char* s, const char* reject);

char* strchr(const char* s, int c);
char* strrchr(const char* s, int c);
char* strstr(const char* haystack, const char* needle);

char* strtok_r(char* restrict s, const char* restrict delim, char** restrict saveptr);
char* strtok(char* restrict str, const char* restrict delim);

char* strerror(int errnum);

char* strcat(char* restrict dst, const char* restrict src);

long long strtoll(const char* restrict nptr, char** restrict endptr, int base);

#endif /* _STRING_H */

