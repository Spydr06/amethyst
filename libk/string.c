#include "string.h"

#include <stdint.h>

void* memset(void* s, int c, size_t n) {
    uint8_t* p = s;
    while(n--)
        *p++ = (uint8_t) c;
    return s;
}

char* reverse(char* str, size_t len) {
    char tmp;
    for(size_t i = 0; i < len / 2; i++) {
        tmp = str[i];
        str[i] = str[len - i - 1];
        str[len - i - 1] = tmp;
    }
    return str;
}

char* utoa(uint64_t num, char* str, int base) {
    size_t i = 0;
    if(num == 0) {
        str[i++] = '0';
        str[i] = '\0';
        return str;
    }

    while(num != 0) {
        uint64_t rem = num % base;
        if(rem > 9)
            str[i++] = ((rem - 10) + 'a');
        else
            str[i++] = rem + '0';
        num /= base;
    }

    str[i] = '\0';
    reverse(str, i);
    return str;
}

char* itoa(int64_t inum, char* str, int base) {
    size_t i = 0;
    if(inum == 0) {
        str[i++] = '0';
        str[i] = '\0';
        return str;
    }

    uint64_t num = (uint64_t) inum;
    if(inum < 0) {
        *str++ = '-';
        num = (uint64_t) -inum;
    }

    while(num != 0) {
        uint64_t rem = num % base;
        if(rem > 9)
            str[i++] = ((rem - 10) + 'a');
        else
            str[i++] = rem + '0';
        num /= base;
    }

    str[i] = '\0';
    reverse(str, i);
    return str;
}


size_t strlen(const char* s) {
    size_t l = 0;
    while(*s++) l++;
    return l;
}

