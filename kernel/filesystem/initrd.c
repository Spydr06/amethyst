#include <filesystem/initrd.h>

#include <init/cmdline.h>
#include <limine/limine.h>
#include <mem/vmm.h>

#include <math.h>
#include <errno.h>
#include <assert.h>
#include <string.h>

static volatile struct limine_module_request module_request = {
    .id = LIMINE_MODULE_REQUEST,
    .revision = 0
};

int initrd_unpack(void) {
    const char* filename = cmdline_get("initrd");
    if(!filename)
        return ENOENT;

    assert(module_request.response);
    
    struct limine_file* initrd = nullptr;
    for(uint64_t i = 0; i < module_request.response->module_count; i++) {
        if(strcmp(module_request.response->modules[i]->path, filename) == 0) {
            initrd = module_request.response->modules[i];
            break;
        }
    }

    if(!initrd) {
        klog(ERROR, "Initrd file `%s` not found.", filename);
        return EBADF;
    }

    klog(INFO, "`%s` at %p with size %lu (%lu pages).", filename, initrd->address, initrd->size, ROUND_UP(initrd->size, PAGE_SIZE) / PAGE_SIZE);

    return 0;
}
