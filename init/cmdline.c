#include <init/cmdline.h>

#include <string.h>
#include <assert.h>

#include <mem/heap.h>
#include <hashtable.h>

static hashtable_t pairs;

void cmdline_parse(size_t cmdline_size, const char* cmdline) {
    assert(hashtable_init(&pairs, 5) == 0);

    if(!cmdline)
        return;

    size_t buffer_size = cmdline_size + 1;
    char buffer[cmdline_size + 1];
    memset(buffer, 0, cmdline_size + 1);

    const char* cmdptr = cmdline;
    char* bufptr = buffer;

    bool convert = true;

    char ch;
    while(*cmdptr) {
        switch(ch = *cmdptr++) {
        case ' ':
            if(convert)
                *bufptr++ = '\0';
            break;
        case '"':
            convert = !convert;
            buffer_size--;
            break;
        default:
            *bufptr++ = ch;
        }
    }

    buffer[buffer_size] = '\0';

    for(size_t i = 0; i < buffer_size;) {
        char* iter = &buffer[i];
        bool pair = false;
        while(*iter) {
            if(*iter == '=') {
                pair = true;
                *iter = '\0';
            }
            iter++;
        }

        size_t keylen = strlen(&buffer[i]);
        if(pair) {
            char* valueptr = &buffer[i + keylen + 1];
            size_t valuelen = strlen(valueptr);

            char* value = kmalloc(valuelen + 1);
            assert(value);
            strcpy(value, valueptr);
            assert(hashtable_set(&pairs, value, &buffer[i], keylen, true) == 0);
            i += valuelen + 1;
        }
        else {
            char* value = kmalloc(keylen + 1);
            assert(value);
            strcpy(value, &buffer[i]);
            assert(hashtable_set(&pairs, value, &buffer[i], keylen, true) == 0);
        }

        i += keylen + 1;
    }
}

const char* cmdline_get(const char* key) {
    void* v;
    return hashtable_get(&pairs, &v, key, strlen(key)) == 0 ? v : nullptr;
}

