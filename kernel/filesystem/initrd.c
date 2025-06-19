#include <filesystem/initrd.h>

#include <ff/tar.h>
#include <init/cmdline.h>
#include <limine.h>
#include <mem/vmm.h>
#include <mem/pmm.h>
#include <mem/heap.h>
#include <filesystem/virtual.h>

#include <math.h>
#include <errno.h>
#include <assert.h>
#include <string.h>

static volatile struct limine_module_request module_request = {
    .id = LIMINE_MODULE_REQUEST,
    .revision = 0
};

static void build_entry(struct tar_entry* entry, void* addr);

int create_parent_dirs(const char *entry_name, struct vattr *entry_attr) {
    int err = 0;

    char *saveptr;
    char *entry = kstrdup(entry_name);
    char *dirname = strtok_r(entry, "/", &saveptr);
    char *next;

    struct vnode *parent_node = vfs_root;

    while((next = strtok_r(NULL, "/", &saveptr))) {
        if(dirname[0] == '\0')
            continue;

        struct vnode *dir_node;
        if(vfs_lookup(&dir_node, parent_node, dirname, NULL, 0) == 0)
            goto next;

        if((err = vfs_create(parent_node, dirname, entry_attr, V_TYPE_DIR, &dir_node)))
            goto cleanup;

    next:
        assert(dir_node != NULL);
        parent_node = dir_node;
        dirname = next;
    }

cleanup:
    kfree(entry);
    return err;
}

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

    klog(INFO, "`%s` at %p with size %Zu (%lu pages).", filename, initrd->address, (size_t) initrd->size, ROUND_UP(initrd->size, PAGE_SIZE) / PAGE_SIZE);
    assert(((uintptr_t) initrd->address % PAGE_SIZE) == 0);

    void* ptr = initrd->address;
    struct tar_entry entry;
    void* cleanup = initrd->address;
    size_t cleaned_bytes = 0;

    while(true) {
        if(cleaned_bytes >= PAGE_SIZE) {
            size_t page_count = cleaned_bytes / PAGE_SIZE;
            pmm_free(FROM_HHDM(cleanup), page_count);
            cleanup = (void*)((uintptr_t) cleanup + ROUND_DOWN(cleaned_bytes, PAGE_SIZE));
            cleaned_bytes %= PAGE_SIZE;
        }

        build_entry(&entry, ptr);

        if(strncmp((const char*) entry.indicator, "ustar", 5))
            break;

        // klog(INFO, "Entry `%s`", entry.name);

        struct vattr entry_attr;
        entry_attr.gid = entry.gid;
        entry_attr.uid = entry.uid;
        entry_attr.mode = entry.mode;

        void* data_start = (void*)((uintptr_t) ptr + TAR_BLOCKSIZE);
        ptr = (void*)((uintptr_t) data_start + ROUND_UP(entry.size, TAR_BLOCKSIZE));
        cleaned_bytes += TAR_BLOCKSIZE;

        int err = create_parent_dirs((const char*) entry.name, &entry_attr);
        if(err) {
            klog(ERROR, "failed to create parent directory of %s: %s (%d)\n", entry.name, strerror(err), err);
            continue;
        }

        struct vnode* node;
        size_t write_count;
        switch(entry.type) {
            case TAR_FILE:
                cleaned_bytes += ROUND_UP(entry.size, TAR_BLOCKSIZE);
                err = vfs_create(vfs_root, (const char*) entry.name, &entry_attr, V_TYPE_REGULAR, &node);
                if(err)
                    break;

                err = vfs_write(node, data_start, entry.size, 0, &write_count, 0);
                vop_release(&node);
                break;
            case TAR_DIR:
                err = vfs_create(vfs_root, (const char*) entry.name, &entry_attr, V_TYPE_DIR, nullptr);
                break;
            case TAR_SYMLINK:
                err = vfs_link(nullptr, (const char*) entry.link, vfs_root, (const char*) entry.name, V_TYPE_LINK, &entry_attr);
                break;
            default:
                klog(ERROR, "unsupported TAR entry type `%s` (%hu)", tar_entry_type_str(entry.type), entry.type);
        }
        
        if(err)
            klog(ERROR, "failed to unpack %s: %s (%d)\n", entry.name, strerror(err), err);
    }

    // free remaining pages
    pmm_free(FROM_HHDM(cleanup), ROUND_UP(cleaned_bytes, PAGE_SIZE) / PAGE_SIZE);

    return 0;
}

static size_t convert(const char* buff, size_t len) {
    size_t result = 0;
    while(len--) {
        result <<= 3;
        result += *buff - '0';
        buff++;
    }

    return result;
}

static void build_entry(struct tar_entry* entry, void* addr) {
    struct tar_header* header = addr;

    char* name_ptr = (char*) entry->name;
    
    if(*header->prefix) {
        size_t len = __last(header->prefix) ? __len(header->prefix) : strlen((const char*) header->prefix);
        memcpy(name_ptr, header->prefix, len);
        name_ptr += len;
        *name_ptr++ = '/';
    }

    size_t name_len = __last(header->name) ? __len(header->name) : strlen((const char*) header->name);
    memcpy(name_ptr, header->name, name_len);
    name_ptr[name_len] = '\0';

#define CONVERT_FIELD(field) (convert((const char*) header->field, __len(header->field) - 1))

    entry->mode = CONVERT_FIELD(mode);
    entry->uid = CONVERT_FIELD(uid);
    entry->gid = CONVERT_FIELD(gid);
    entry->size = CONVERT_FIELD(size);
    entry->modtime.s = CONVERT_FIELD(modtime);
    entry->modtime.ns = 0;
    entry->checksum = CONVERT_FIELD(checksum);
    entry->type = convert((const char*) header->type, 1);
    entry->devmajor = CONVERT_FIELD(devmajor);
    entry->devminor = CONVERT_FIELD(devminor);

#undef CONVERT_FIELD

    if(__last(header->link)) {
        memcpy(entry->link, header->link, __len(header->link));
        __last(entry->link) = '\0';
    }
    else
        strcpy((char*) entry->link, (const char*) header->link);

    memcpy(entry->indicator, header->indicator, __len(header->indicator));
    memcpy(entry->version, header->version, __len(header->version));
}
