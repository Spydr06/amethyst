#include <init/cmdline.h>

#include <string.h>
#include <assert.h>
#include <hashtable.h>

static hashtable_t pairs;

void cmdline_parse(size_t cmdline_size, const char* cmdline) {
    assert(hashtable_init(&pairs, 5) == 0);

    if(!cmdline)
        return;

}

const char* cmdline_get(const char* key) {
    void* v;
    return hashtable_get(&pairs, &v, key, strlen(key)) == 0 ? v : nullptr;
}

