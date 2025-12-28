#include <init/module.h>
#include <amethyst/module.h>

#include <encoding/elf.h>
#include <hashtable.h>
#include <sys/spinlock.h>
#include <mem/heap.h>
#include <mem/slab.h>

#include <math.h>
#include <assert.h>
#include <errno.h>
#include <string.h>
#include <kernelio.h>

#define KMODULE_MAX 128

static volatile bool module_table_initialized = false;
static spinlock_t module_table_lock;
static hashtable_t module_table;

static struct scache* kmodule_mapping_scache;
static struct scache* kmodule_scache;

static void kmodule_ctor(struct scache*, void* ptr) {
    struct kmodule *kmod = (struct kmodule*) ptr;
    memset(kmod, 0, sizeof(struct kmodule));
}

static void kmodule_dtor(struct scache*, void* ptr) {
    struct kmodule *kmod = (struct kmodule*) ptr;
    spinlock_acquire(&kmod->lock);
    assert(kmod->initialized == false);

    if(kmod->spec && kmod->spec->name) {
        spinlock_acquire(&module_table_lock);
        hashtable_remove(&module_table, (void *)kmod->spec->name, strlen(kmod->spec->name));
        slab_free(kmodule_scache, kmod);
        spinlock_release(&module_table_lock);
    }

    kmodule_mapping_release(kmod->mapping);
}

static void kmodule_mapping_ctor(struct scache*, void* ptr) {
    struct kmodule_mapping* kmod = (struct kmodule_mapping*) ptr;
    memset(kmod, 0, sizeof(struct kmodule_mapping));
    kmod->refcount = 1;
}

static void kmodule_mapping_dtor(struct scache*, void* ptr) {
    struct kmodule_mapping* map = (struct kmodule_mapping*) ptr;

    if(map->args) {
        for(char *arg = map->args[0]; arg; arg++)
            kfree(arg);
        kfree(map->args);
    }

    for(Elf64_Half i = 0; i < map->ehdr.e_shnum; i++) {
        kmodule_unload_section(map, i);
    }

    kfree((void*) map->shdrs);
    kfree((void*) map->sections);
    if(map->vnode)
        vop_release(&map->vnode);
}

static int init_module_table(void) {
    if(!__sync_bool_compare_and_swap(&module_table_initialized, false, true))
        return 0;

    spinlock_init(module_table_lock);
    
    int err;
    if((err = hashtable_init(&module_table, KMODULE_MAX)))
        return err;

    kmodule_scache = slab_newcache(sizeof(struct kmodule), _Alignof(struct kmodule), kmodule_ctor, kmodule_dtor);
    assert(kmodule_scache != nullptr);

    kmodule_mapping_scache = slab_newcache(sizeof(struct kmodule_mapping), _Alignof(struct kmodule_mapping), kmodule_mapping_ctor, kmodule_mapping_dtor);
    assert(kmodule_mapping_scache != nullptr);

    return err;
}

static int load_section_headers(Elf64_Shdr **shdrs, struct vnode *node, const Elf64_Ehdr *header) {
    if(header->e_shentsize != sizeof(Elf64_Shdr))
        return EINVAL;

    if(header->e_shstrndx >= header->e_shnum) {
        klog(ERROR, "ELF file does not have '.shstrtab' section.");
        return EINVAL;
    }

    size_t shtable_size = header->e_shentsize * header->e_shnum;
    if(!(*shdrs = kmalloc(shtable_size)))
        return ENOMEM;

    return elf_read_exact(node, *shdrs, shtable_size, header->e_shoff);
}

static const char *get_section_name(const struct kmodule_mapping *map, uintptr_t *sections, Elf64_Half idx) {
    if(idx >= map->ehdr.e_shnum)
        return nullptr;

    if(sections[map->ehdr.e_shstrndx] == 0)
        return nullptr;

    const Elf64_Shdr* shdr = map->shdrs + idx;
    if(shdr->sh_name >= map->shdrs[map->ehdr.e_shstrndx].sh_size)
        return nullptr;

    return (const char*) map->sections[map->ehdr.e_shstrndx] + shdr->sh_name;
} 

void kmodule_mapping_delete(struct kmodule_mapping *map) {
    slab_free(kmodule_mapping_scache, map);
}

static Elf64_Half find_section(const struct kmodule_mapping *map, const char *name) {
    for(Elf64_Half i = 0; i < map->ehdr.e_shnum; i++) {
        const char *sh_name = get_section_name(map, map->sections, i);
        if(!sh_name)
            continue;
        if(strcmp(name, sh_name) == 0)
            return i;
    }

    return 0;
}

static int kmodule_init(struct kmodule *kmod, size_t argc, char **args) {
    spinlock_acquire(&kmod->lock);

    int err = 0;
    if(!__sync_bool_compare_and_swap(&kmod->initialized, false, true))
        goto cleanup;

    klog(INFO, "Loaded kernel module '%s' [v%s, %s License]...",
        kmod->spec->name, kmod->spec->version, kmod->spec->license);

    int ret = kmod->spec->main_func(argc, (const char **)args);
    if(ret != 0) {
        klog(INFO, "%s::main() returned with exit code '%d'.", kmod->spec->name, ret);
        err = EINVAL;
        goto cleanup;
    }

cleanup:
    if(err != 0)
        kmod->initialized = false;
    spinlock_release(&kmod->lock);
    return 0;
}

int kmodule_load(struct vnode *node, size_t argc, char **args, enum amethyst_module_flags flags __unused) {
    int err;
    if((err = init_module_table()))
        return err;

    struct kmodule_mapping* map = slab_alloc(kmodule_mapping_scache);
    if(!map)
        return ENOMEM;

    vop_hold(node);
    map->vnode = node;
    map->args = args;

    if((err = elf_read_exact(node, &map->ehdr, sizeof(Elf64_Ehdr), 0)))
        return err;

    if(!elf_validate_ehdr(&map->ehdr, ET_REL))
        return ENOEXEC;

    if((err = load_section_headers(&map->shdrs, node, &map->ehdr)))
        goto cleanup;

    if(!(map->sections = kcalloc(map->ehdr.e_shnum, sizeof(uintptr_t)))) {
        err = ENOMEM;
        goto cleanup;
    }

    if((err = kmodule_load_section(map, map->ehdr.e_shstrndx))) {
        klog(ERROR, "Could not load section '.shstrtab' [%u]", map->ehdr.e_shstrndx);
        goto cleanup;
    }

    for(Elf64_Half i = 0; i < map->ehdr.e_shnum; i++) {
        if(i == map->ehdr.e_shstrndx)
            continue;
        if((err = kmodule_load_section(map, i))) {
            klog(ERROR, "Could not load section '%s' [%u]: %s", get_section_name(map, map->sections, i), i, strerror(err));
            goto cleanup;
        }
    }

    for(Elf64_Half i = 0; i < map->ehdr.e_shnum; i++) {
        if((err = kmodule_reloc_section(map, i))) {
            klog(ERROR, "Could not relocat section '%s' [%u]: %s", get_section_name(map, map->sections, i), i, strerror(err));
            goto cleanup;
        }
    }


    Elf64_Half modinfo_idx = find_section(map, AMETHYST_MODINFO_SECTION);
    if(!modinfo_idx) {
        klog(ERROR, "Module file does not contain section '%s'.", AMETHYST_MODINFO_SECTION);
        err = EINVAL;
        goto cleanup;
    }

    if(map->shdrs[modinfo_idx].sh_size == 0) {
        klog(ERROR, "Module file has empty '%s' section.", AMETHYST_MODINFO_SECTION);
        err = EINVAL;
        goto cleanup;
    }

    size_t spec_memsize = ROUND_UP(sizeof(struct amethyst_module_spec), _Alignof(struct amethyst_module_spec));
    size_t spec_count = map->shdrs[modinfo_idx].sh_size / spec_memsize;
    for(size_t i = 0; i < spec_count; i++) {
        const struct amethyst_module_spec* spec = (const struct amethyst_module_spec*)(map->sections[modinfo_idx] + i * spec_memsize);
        if(spec->magic != AMETHYST_MODINFO_MAGIC) {
            klog(ERROR, "Module specification [%zu] does not have correct magic bytes (got %lx, expect %llx)",
                i, spec->magic, AMETHYST_MODINFO_MAGIC);
            err = EINVAL;
            goto cleanup;
        }

        if(!spec->name) {
            klog(ERROR, "Module specification [%zu] does not have a valid name.", i);
            goto cleanup;
        }

        spinlock_acquire(&module_table_lock);

        struct kmodule *kmod = slab_alloc(kmodule_scache);
        if(!kmod) {
            err = ENOMEM;
            goto cleanup;
        }

        kmodule_mapping_hold(map);
        kmod->spec = spec;
        kmod->mapping = map;

        const struct kmodule *existing;
        if(!hashtable_get(&module_table, (void**) &existing, spec->name, strlen(spec->name))) {
            slab_free(kmodule_scache, kmod);

            if(existing->mapping == map)
                continue; // already loaded
            klog(ERROR, "Module with name '%s' is already registered.", spec->name);
            err = EEXIST;
            goto cleanup;
        }
        
        if((err = hashtable_set(&module_table, map, spec->name, strlen(spec->name), true))) {
            slab_free(kmodule_scache, kmod);
            goto cleanup;
        }

        spinlock_release(&module_table_lock);

        if((err = kmodule_init(kmod, argc, args))) {
            slab_free(kmodule_scache, kmod);
            goto cleanup;
        }
    }

cleanup:
    kmodule_mapping_release(map);
    return err;
}

void kmodule_unload(struct kmodule *kmod) {
    spinlock_acquire(&kmod->lock);
    if(kmod->spec->cleanup_func)
        kmod->spec->cleanup_func();

    kmod->initialized = false;
    spinlock_release(&kmod->lock);

    slab_free(kmodule_scache, kmod);
}

const struct kmodule *kmodule_query(const char *name) {
    if(!module_table_initialized)
        return nullptr;

    spinlock_acquire(&module_table_lock);

    struct kmodule* module = nullptr;
    hashtable_get(&module_table, (void**) &module, name, strlen(name));

    spinlock_release(&module_table_lock);
    return module;
}
