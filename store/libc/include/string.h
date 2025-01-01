#ifndef _STRING_H
#define _STRING_H

#include <bits/alltypes.h>

#ifdef __cplusplus
extern "C" {
#endif

#if __cplusplus >= 201103L
    #define NULL nullptr
#elif defined(__cplusplus)
    #define NULL 0L
#else
    #define NULL ((void*) 0)
#endif

void* memcpy(void* restrict d, const void* restrict s, size_t n);
void* memmove(void* d, const void* restrict s, size_t n);
void* memset(void* s, int c, size_t n);

void* memchr(const void* s, int c, size_t n);
void* memrchr(const void* s, int c, size_t n);

int memcmp(const void* s1, const void* s2, size_t n);

char* strcpy(char* restrict d, const char* restrict s);
char* strncpy(char* restrict d, const char* restrict s, size_t n);

char* strcat(char* restrict d, const char* restrict s);
char* strncat(char* restrict d, const char* restrict s, size_t n);

int strcmp(const char* s1, const char* s2);
int strncmp(const char* s1, const char* s2, size_t n);

int strcoll(const char* s1, const char* s2);
size_t strxfrm(char* restrict d, const char* restrict s, size_t n);

char* strchr(const char* s, int c);
char* strrchr(const char* s, int c);

size_t strspn(const char* d, const char* accept);
size_t strcspn(const char* d, const char* reject);

char* strpbrk(const char* s, const char* accept);
char* strstr(const char* haystack, const char* needle);
char* strtok(char* restrict s, const char* restrict delim);

size_t strlen(const char* s);

char* strerror(int _errno);

#if defined(_BSD_SOURCE) || defined(_GNU_SOURCE)
    char* strsep(char** sp, const char* delim);

    size_t strlcat(char* d, const char* s, size_t n);
    size_t strlcpy(char* d, const char* s, size_t n);
    
    #if defined(_XOPEN_SOURCE)
        void* memccpy(void* restrict d, const void* restrict s, int c, size_t n);
    #endif

    #if defined(_XOPEN_SOURCE) || defined(_POSIX_SOURCE) || defined(_POSIX_C_SOURCE
        void* memmem(const void* haystack, size_t haystacklen, const void* needle, size_t needlelen);

        char* strtok_r(char* restrict s, const char* restrict delim, char** restrict saveptr);
        int strerror_r(int _errno, char* d, size_t n);

        char* stpcpy(char* restrict d, const char* restrict s);
        char* stpncpy(char* restrict d, const char* restrict s, size_t n);

        size_t strnlen(const char* s, size_t n);

        char* strdup(const char* s);
        char* strndup(const char* s, size_t n);

        char* strsignal(int sig);
    #endif
#endif

#if defined(_GNU_SOURCE)
    void* memrchr(const void* s, int c, size_t n);
    void* mempcpy(void* d, const void* s, size_t n);
    
    #define strdupa(s)  (strcpy(alloca(strlen((s)) + 1), (s)))

    int strverscmp(const char* s1, const char* s2);
    char* strchrnul(const char* s, int c);
    char* strcasestr(const char* s1, const char* s2);
#endif

#ifdef __cplusplus
}
#endif

#endif /* _STRING_H */

